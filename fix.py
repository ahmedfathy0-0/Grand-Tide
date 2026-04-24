import re

with open('config/app.jsonc', 'r') as f:
    data = f.read()

# We just want to append to the shark components and add a spawner right after the shark
data = re.sub(
    r'(\{\s*"type"\s*:\s*"Health",\s*"maxHealth"\s*:\s*100,\s*"currentHealth"\s*:\s*100\s*\})\s*\]\s*\}',
    r'{\n                        "type": "Enemy",\n                        "enemyType": "shark",\n                        "speed": 15.0,\n                        "attackRange": 30.0,\n                        "damage": 20.0,\n                        "state": 0,\n                        "timer": 0.0,\n                        "primaryTarget": "RAFT"\n                    },\n                    \1\n                ]\n            },\n            {\n                "name": "marine_spawner",\n                "position": [0, 0, 0],\n                "components": [\n                    {\n                        "type": "Enemy",\n                        "enemyType": "spawner",\n                        "maxSpawns": 3\n                    }\n                ]\n            }',
    data
)

with open('config/app.jsonc', 'w') as f:
    f.write(data)
