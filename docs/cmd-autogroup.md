---
tags:
  - command
---

# /autogroup

## Syntax

<!--cmd-syntax-start-->
```eqcommand
/autogroup [option] [setting]
```
<!--cmd-syntax-end-->

## Description

<!--cmd-desc-start-->
Configures MQ2AutoGroup much faster than writing an .ini from scratch.
<!--cmd-desc-end-->

## Options

| Option | Description |
|--------|-------------|
| `create` | Create a new group, this player will be the leader. |
| `delete` | Will delete the group this player was in. |
| `startcommand <Command to be executed>` | Will execute the command once your group is formed. |
| `set [maintank|mainassist|puller|marknpc|masterlooter]` | Will set the player/merc targeted to that group role. |
| `remove [maintank|mainassist|puller|marknpc|masterlooter]` | Will remove that group role from your group. |
| `add [player|merc]` | Add the player/merc of the player targeted to your group. |
| `remove [player|merc]` | Remove the player/merc of the player targeted from their group. |
| `add [eqbc|dannet]` | Adds a character connected to eqbc/dannet to your group upon next login. If you want multiple group slots, issue this command multiple times. |
| `remove [eqbc|dannet]` | Will remove the the option to invite characters connected to eqbc/dannet. |
| `status` | Displays your settings and group. |
| `help` | displays help text |

## Examples

`/autogroup add dannet`

This will add a character connected to [MQ2DanNet](MQ2DanNet) to the group upon next login. If you want a full group of DanNet characters, type the above command five times.
