# VISION.md

We will be creating a prototype of the game mechanic in Unreal Engine 5.

I'm creating a driving system for my game. One of the important aspects of my game is driving but instead of focusing on speeding on a highway or a race track, I want to create a driving system for going into an offroad in your old, clunky jeep.

## Overview

The game striving is inspired by
- Uncharted 4 madagascar section with winch
- Mudrunner/Snowrunner
- Death Stranding

The game has two modes:
- Driving in the car (the main one)
- FPP, where you can get out of the car, to see the surroundings, find the path, unlock something for the car etc.

I want to influence it heavy on games like Mudrunner and Snowrunner, but instead of driving heavy trucks for heavy duty work, the game is more personal. It is game of personal stakes of traveling through post-apocaliptic world. You and your old jeep are traveling through rough terrain: mud, swamp, rivers, old, destroyed roads, devastated buildings overrun by vegetation, woods, bushes etc. You have to travel from one point to another in the world, but to get there, you have to go through obstacles, to the point where game becomes about problem solving. Something like transporting good in Death Stranding.

## Example we want to build in the end

To give you an example, here is one of the situations in the game. This is what I want to build with this prototype.
- We were tasked to go from a town called "Huta" to the settlement called "Bagnisko". The road there goes through the river "Rwąca".
- When the player reaches the river in his trusted, old jeep, they see that the bridge on the river has collapsed. Fortunatelly, there is a small path that leeds to the woods. The player turns their car and drives through the woods. In the woods the path is bumpy and muddy. The car stucks in the mood a couple of times, and the player needs to drive slowly for two reasons: 1) the faster they try to go, The more car's wheels are spinning in the mud and the player can't drive, so the player needs to drive slowly and with intent, 2) then the road becomes bumpy, with a lot of tree branches - bumps on the road and tree branches, are damaging the car "health" when the player drives faster.
- Finally the player reaches the ford on the river. They have two possible paths to cross the river. 1) First path is the deeper river: The player can go straight into a river, but the current will be strong and the car will be pushed sideways - the jeep will be very hard to navigate and player most likely will be pushed to the path 2. 2) Mud on shallow river. Mud will spring from the wheels, and the car will stop as it will get stuck in deep mud. This part is inspired by Uncharted 4 mud sections and Mudrunner.
- Fortunatelly, we have a tool that can help us overcome these obstacles: A winch (as in Uncharted 4). Before crossing the river or after the car will get stuck in the mood, the player can get out of the car, go to the front of it, grab a winch and cross the river on foot. As we are still in the woods, the player can hood the winch's cable to the large tree. Now he can go back to the car, start the engine and try crossing now. They can use now the winch controlls, to increase the pull and together with car engine get out of the mud or fight the current of the river.

So this will be physics-heavy driving system. My physics requirements:
- Mud: Slows the car, sometimes to full halt. The wheels spins in it driving less progress the deeper we are burried in it. The mud springs from the car tires. When driving through mud, the car is leaving the trails in the mud with the tires.
- Trees: Smaller trees and bushes can be knocked over with the car. Larger trees are hard obstacles that can damage car when we collide with more speed. The branches are reacting to car as well - smaller branches should bent when the car goes through bushes, they will also scratch the car and damage it if we go to fast. The larger baranches require more force to break (going faster so some damage).
- Water: strong current can push the car.

For all of these I don't always need a full physics simulation. Some can be done with cheap tricks. E.g. a river has a box collision which on overlap, applies side force to the car. It does not need a water simulation. Similar for other situations: clever tricks are welcome.

## Game technical overview

1. **Gameplay Ability System.** I want to base my game on GAS. So when in doubt, use patterns from this framework.
2. **Best practices** I want to develop using good UE5 practices. When in doubt Use LyraStarterGame for source of good practices.
3. **Physics, not great graphics** I don't care that much about AAA quality of graphics, in fact I want to have all recources for physics calculation and keep the graphics simple. To the degree that physics make sense of course. I want to have mud effects launching from tires when they spin in the mud, but I don't care about ray tracing lights etc. I have a low-end machine and I want the game to run on it.
4. **Debug tools** I want to have proper debug tools for features that we are building. I want to use NetImGui as primary debugging tool, but other tools that good are: `ShowDebug` command to display text over a vehicle or obstacle. Cheat commands, Utility widgets to modify or track parameters (if can't be done with NetImGui).