# Fight Night

Demo Video Link: 

## Overview

Fight Night is a two-player co-op fighting game built in Unreal Engine 5. The core concept is that two players share control of a single fighter, with each player responsible for different parts of the character.
One player controls movement and defensive actions, while the other controls offensive combat.
The project is currently in active development and is being expanded with additional gameplay systems, AI improvements, and networking refinements so feel free to check back for updates!

Each fighter is controlled by two players:

- Legs Player: Responsible for movement, positioning, and dodging/sliding 
- Arms Player: Responsible for attacks and offensive actions

A match is won when a fighter’s health is reduced to zero. 

---

## Controls

### Legs Player (Movement / Defense)

- WASD: Move the fighter  
- Double-tap A/S/D: Slide Left, Back, or Right 

Responsibilities:
- Movement and positioning  
- Dodging and evasion  
- Maintaining spacing and survival  

---

### Arms Player (Combat / Offense)

- Q / E: Perform left and right jabs  
- A / D: Perform hook attacks  

Responsibilities:
- Offensive pressure  
- Attack timing and sequencing  
- Creating opportunities for knockdowns
  
---

## Key Systems

### Multiplayer / Networking

The project is built around Unreal Engine’s networking framework and uses an authoritative server model to ensure consistent gameplay across clients.

Key aspects include:
- Replicated character state across all clients  
- Server-authoritative combat resolution  
- Separation of input sources across two players controlling a single character  
- Synchronized movement, attack events, damage, and knockdown states  

A key technical challenge in this system is supporting two independent input streams that jointly control a single character while maintaining responsiveness and consistency across the network.

---

### AI System

The game includes an AI opponent designed to operate under the same constraints as player-controlled fighters.

The AI system handles:
- Movement decisions based on distance and engagement state  
- Attack selection and timing  
- Defensive positioning and disengagement behavior
- "Aggression" factor giving the AI a different behaviour depending on the amount of health it has

The AI is integrated directly into the same combat and movement systems used by players, ensuring consistent gameplay logic between human and AI-controlled opponents.

---

## Behavior Tree (AI)

<img width="699" height="933" alt="image" src="https://github.com/user-attachments/assets/f41d68c0-87cf-4ae1-85d3-f2a303968b84" />

The behaviour tree is very simple for this project not because of the simplicity of the AI but rather because I decided a utility based AI where I evaluate each action's score at each moment and see which one is best suited for the character. 
So instead of just following a tree, the AI will calculate scores based on these parameters within the utility selector.

<img width="704" height="1140" alt="image" src="https://github.com/user-attachments/assets/1a4d405a-a8f1-44ba-a0d3-f9282d57fc81" />

---


