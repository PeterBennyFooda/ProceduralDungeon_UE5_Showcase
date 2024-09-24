# Procedural Dungeon Generator (Unreal Engine 5)

A procedural dungeon generation system in Unreal Engine 5 using C++, incorporating Delaunay triangulation and minimum spanning tree algorithms to optimize paths between rooms. 

https://github.com/user-attachments/assets/3df05ac0-1c41-4a6a-9190-f40d2b48f87b

## Features

This system generates complex, multi-layered dungeons with customizable parameters, such as dungeon size, number of staircases per floor, and minimum room count per floor.   

It also supports prefabs, allowing users to create a list of room, hallway, and staircase blueprints within the engine, which are then procedurally generated at runtime.
Optionally, the user can generate the dungeon within a building, with the building's size determined by the dungeon size parameters set by the user.

<img src= "https://github.com/user-attachments/assets/a75b8a30-82d0-4411-8062-a84fbf6fcc4a" width ="382.5" height="288">  
<img src= "https://github.com/user-attachments/assets/1b26ab7d-5252-4b79-9ad9-f1dd9b9aafc5" width ="382.5" height="288">  
<img src= "https://github.com/user-attachments/assets/bbe0fbff-b968-4617-ae8c-b4be06711b7c" width ="382.5" height="288">  
<img src= "https://github.com/user-attachments/assets/36ac92d9-4f08-425f-99cd-c719874e8298" width ="382.5" height="288"> 

![#1589F0](https://placehold.co/15x15/1589F0/1589F0.png) `Room`, ![#f03c15](https://placehold.co/15x15/f03c15/f03c15.png) `Staircase`, ![#c5f015](https://placehold.co/15x15/c5f015/c5f015.png) `Hallway`, ![#eeeeee](https://placehold.co/15x15/eeeeee/eeeeee.png)`Wall & Celling` 

## Technical Details

### Step 1
Position rooms randomly while ensuring they do not overlap.

### Step 2
Create a **Delaunay triangulation** graph of the rooms. 

### Step 3
Generate a **minimum spanning tree (MST)** from the triangulation, ensuring all rooms are connected and accessible. Since the MST forms a tree, it contains no cycles, providing only one path between any two rooms.

### Step 4
Create a list of hallways, initially including all edges from the tree generated in Step 3. Since the tree connects all rooms, it ensures a path to every room exists. Then, **randomly add additional edges** from the triangulation graph to the list.

### Step 5
For each hallway in the list, the **A\* algorithm** is used to find a path from its start to its end. Once a path is found, it modifies the world state, allowing future hallways to navigate around existing ones.
The cost function prioritizes using hallways carved in previous iterations, making it cheaper to extend existing paths rather than create new ones.

### Step 6
Generate decorative objects such as furniture, walls, and ceilings.
