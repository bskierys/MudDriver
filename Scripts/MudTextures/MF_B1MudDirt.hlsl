// Custom node body of MF_B1MudDirt (keep in sync with the asset). Returns float4(base colour, dirt mask).
// P / Nl: local position and normal. Body: D0 = body dirt, D1..D4 = wheels FL FR BL BR.
// Tire: D0 = slow dry crust, D1 = mud coat (0.4 = tread covered, 1 = thick, reaching the rim), D2 = wetness.
float dmax = max(max(D0, max(D1, D2)), max(D3, D4));
if (dmax <= 0.0) return float4(BC, 0.0);

float3 an = pow(abs(Nl), 4.0);
an /= (an.x + an.y + an.z + 1e-5);
float2 ux = float2(P.y, -P.z);
float2 uy = float2(P.x, -P.z);
float2 uz = float2(P.x, P.y);
float b = Texture2DSample(Blotch, BlotchSampler, ux / TileB).r * an.x
        + Texture2DSample(Blotch, BlotchSampler, uy / TileB).r * an.y
        + Texture2DSample(Blotch, BlotchSampler, uz / TileB).r * an.z;
float s = Texture2DSample(Splat, SplatSampler, ux / TileS).r * an.x
        + Texture2DSample(Splat, SplatSampler, uy / TileS).r * an.y
        + Texture2DSample(Splat, SplatSampler, uz / TileS).r * an.z;
float n = Texture2DSample(Noise, NoiseSampler, ux / TileN).r * an.x
        + Texture2DSample(Noise, NoiseSampler, uy / TileN).r * an.y
        + Texture2DSample(Noise, NoiseSampler, uz / TileN).r * an.z;
float field = lerp(b, n, 0.35);
float3 mud = MudColor * lerp(0.75, 1.15, n);

if (IsTire > 0.5)
{
    // lighter than on paint, or it vanishes on black rubber
    mud *= 1.6;
    float crust = D0 * Scale * 1.25;
    float mCrust = saturate((field - (1.0 - crust)) / Soft) * step(1e-4, crust);

    // Coat grows from the tread inwards: the band edge moves from TreadR to RimR as it thickens past the tread stage.
    float coat = D1;
    float r = length(P.xz);
    float edge = lerp(TreadR, RimR, saturate((coat - 0.4) / 0.6));
    float band = saturate((r - edge) / 3.0 + (n - 0.5) * 0.6);
    float cover = saturate(coat / 0.4);
    float mCoat = band * saturate((lerp(b, n, 0.5) - (1.0 - cover * 1.1)) / Soft) * step(1e-4, coat);
    float3 mudCoat = mud * lerp(1.0, WetDarken, D2);

    float3 tireColor = lerp(lerp(BC, mud, mCrust), mudCoat, mCoat);
    return float4(tireColor, max(mCrust, mCoat));
}

float D0s = D0 * Scale;
float4 Dw = float4(D1, D2, D3, D4) * Scale;
float4 a = saturate(1.0 - (float4(length(P - A0), length(P - A1), length(P - A2), length(P - A3)) - ArchR) / ArchFade);
float4 aw = a * Dw;
float cArch = max(max(aw.x, aw.y), max(aw.z, aw.w)) * 1.3;
float hSill = saturate(1.0 - (P.z - BMin.z) / max(Sill - BMin.z, 1.0));
float cSill = hSill * max(D0s * 1.5, dot(Dw, 0.25) * 0.9);
float hz = 1.0 - saturate((P.z - BMin.z) / max(BMax.z - BMin.z, 1.0));
float cLow = D0s * hz * hz * 1.1;
float cB = saturate(max(max(cArch, cSill), cLow));
float mB = saturate((field - (1.0 - cB)) / Soft) * step(1e-4, cB);

float cSplat = D0s * (0.3 + 1.2 * hz * hz);
float cSpots = saturate((D0s - 0.7) / 0.3) * 0.35;
float cS = saturate(max(max(cSplat, cSpots), cArch * 0.8));
float mS = s > 0.002 ? saturate((s - (1.0 - cS)) / Soft) * step(1e-4, cS) : 0.0;
float m = max(mB, mS);
// dried dust over the whole body late on, patchy by the noise
float film = saturate((D0s - 0.3) / 0.7) * FilmStrength * lerp(0.5, 1.0, n);

float3 outColor = lerp(lerp(BC, mud, film), mud, m);
return float4(outColor, max(m, film));
