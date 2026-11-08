// M_B1LensMud Custom node (ADR-022). Keep in sync with the node in /Game/B1/FX/M_B1LensMud.
// Output float3. Inputs: UV (ViewportUV), ViewSize, Scene (PostProcessInput0 color; also binds the input for SceneTextureLookup),
// Drops (texture object), S0..S3 (slot: strength, offset U, offset V, variant), Refraction, DropFill, DropSoftness,
// DropBlur, DropOpacity, DropTint, DropTintAmount, Highlight, BottomScale, TopDensity, RevealGamma.
float3 Col = Scene.rgb;
float4 Slots[4] = { S0, S1, S2, S3 };
if (max(max(S0.x, S1.x), max(S2.x, S3.x)) <= 0.0)
{
	return Col;
}

float Aspect = ViewSize.x / max(ViewSize.y, 1.0);
// Lower screen: drops bigger (tile sampled zoomed in) and denser, where most of the mud lands.
float YW = UV.y * UV.y;
float Zoom = 1.0 / lerp(1.0, BottomScale, YW);
float Density = lerp(TopDensity, 1.0, YW);
// Tile height = screen height; the tile is 2:1.
float2 Base = float2(UV.x * Aspect * 0.5, UV.y) * Zoom;
float BestA = 0.0;
float BestMud = 0.0;
float2 BestN = 0.0;

for (int i = 0; i < 4; i++)
{
	float4 S = Slots[i];
	float K = S.x;
	if (K <= 0.0)
	{
		continue;
	}
	int V = (int)round(S.w);
	float2 T = frac(Base + S.yz);
	T.x = (V & 1) ? 1.0 - T.x : T.x;
	T.y = (V & 2) ? 1.0 - T.y : T.y;
	bool bInv = (V & 4) != 0;

	// B = 1 - normalized radius: the drop shrinks as the slot fades, and its edge is soft.
	// The reveal is front-loaded (RevealGamma < 1), so the first splashes already show a few drops.
	float4 D = Drops.SampleLevel(DropsSampler, T, 0);
	float DId = (bInv && D.b > 0.0) ? 1.0 - D.a : D.a;
	float Thr = max(1.0 - K * DropFill, 0.0);
	float DA = (DId < pow(K, RevealGamma) * Density && D.b > 0.0) ? saturate((D.b - Thr) / max(DropSoftness, 0.01)) : 0.0;
	if (DA > BestA)
	{
		BestA = DA;
		BestMud = lerp(0.35, 1.0, frac(DId * 13.7 + 0.31));
		BestN = D.rg * 2.0 - 1.0;
		BestN.x = (V & 1) ? -BestN.x : BestN.x;
		BestN.y = (V & 2) ? -BestN.y : BestN.y;
	}
}

if (BestA > 0.0)
{
	float2 Px = float2(1.0 / Aspect, 1.0);
	float2 RUV = UV - BestN * Refraction * Px;
	float2 O = DropBlur * Px;
	float2 Taps[9] = { float2(0, 0), float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1),
		float2(0.7, 0.7), float2(-0.7, 0.7), float2(0.7, -0.7), float2(-0.7, -0.7) };
	float3 Refr = 0.0;
	for (int j = 0; j < 9; j++)
	{
		Refr += SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(RUV + Taps[j] * O, 14), 14), 14, true).rgb;
	}
	Refr /= 9.0;
	// Mud settles at the lower edge; some drops are muddier than others. No dark rim.
	float NL2 = saturate(dot(BestN, BestN));
	float Bottom = saturate(0.4 + 0.8 * BestN.y);
	float TintW = saturate(DropTintAmount * BestMud * (0.35 + 0.65 * NL2 * Bottom));
	float3 DCol = lerp(Refr, DropTint.rgb, TintW);
	float2 HN = BestN + float2(0.3, 0.4);
	DCol += Highlight * exp(-dot(HN, HN) / 0.08);
	Col = lerp(Col, DCol, BestA * DropOpacity);
}
return Col;
