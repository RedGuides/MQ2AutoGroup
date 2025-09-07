---
tags:
  - plugin
resource_link: "https://www.redguides.com/community/resources/mq2autogroup.91/"
support_link: "https://www.redguides.com/community/threads/mq2autogroup.54769/"
repository: "https://github.com/RedGuides/MQ2AutoGroup"
config: "MQ2AutoGroup.ini"
authors: "plure, knightly, brainiac"
tagline: "will create your group, set group roles, and then run a command"
quick_start: "https://www.redguides.com/community/resources/mq2autogroup.91/"
---

# MQ2AutoGroup
<!--desc-start-->
MQ2AutoGroup is a plugin created by plure for RedGuides. It will create your group, set group roles, and then run a command. You can configure it through the commandline or through an .ini file.
<!--desc-end-->

## Commands

<a href="cmd-autogroup/">
{% 
  include-markdown "projects/mq2autogroup/cmd-autogroup.md" 
  start="<!--cmd-syntax-start-->" 
  end="<!--cmd-syntax-end-->" 
%}
</a>
:    {% include-markdown "projects/mq2autogroup/cmd-autogroup.md" 
        start="<!--cmd-desc-start-->" 
        end="<!--cmd-desc-end-->" 
        trailing-newlines=false 
     %} {{ readMore('projects/mq2autogroup/cmd-autogroup.md') }}

## Settings

Sample MQ2AutoGroup.ini

```ini
[StartCommand]
firiona_Hoosierbilly=/macro startshit
firiona_Eqmule=/macro startshit
firiona_Redbot=/macro startshit
firiona_Plure=/macro startshit
firiona_Masuri=/macro startshit
firiona_Noobhaxor=/macro startshit
[Group1]
Server=firiona
Member1=Hoosierbilly|Master Looter
Member2=Eqmule
Member3=Redbot|Main Tank|Main Assist
Member4=Plure
Member5=Masuri
Member6=Noobhaxor
```

Member1 is always going to be leader of the group. He is going to invite the other when they are in the same zone, they will only be invited one time. As people join the leader will assign group roles based on what you set in the ini. If for some reason your leader isn't Member1, no group roles will be assigned.

Group slots can be reserved for mercenaries. If you set one of your characters to use their mercenary they will attempt to revive them if dead, or if you don't set a character to have a mercenary and they are up they will be suspended. This action is only done right away, it doesn't continue through your time playing.

Open group slots can be set aside to be used by anyone connected to your eqbc server or on dannet. The group leader will invite anyone that connected to either service that is in the same zone as the group leader.

After all of the group members join, or you invite someone manually and the group is full each character will run one command that you have set for them. I have all of my characters run start.mac, it decides what macro to run by the zone your in, puts in a delay for my puller to give the buffers time to do their buffing etc.

## See also

- [Sic's overview guide](https://www.youtube.com/watch?v=uxgdlkq2LTY)
