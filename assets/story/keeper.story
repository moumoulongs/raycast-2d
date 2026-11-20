; THE KEEPER OF THE DROWNED LIGHT — Bilingual Story Script (ZH / EN)
; Flag map:
;   1 = Dock Signal Brazier lit (lowers Sea-Gate 'D')
;   2 = Entered R'lyeh Sanctum (auto enter trigger)
;   3 = West Seal rekindled (rises West Shortcut Bridge 'U')
;   4 = East Seal rekindled (rises East Shortcut Bridge 'V')
;   5 = North Seal rekindled
;   6 = All 3 Seals active (auto-set by StoryRuntime -> lowers Central Altar barrier 'S')
;   7 = Hidden Unsigned Note #0 discovered behind Lighthouse
;   8 = Pale Lantern Echo forgiven in East Choir
;   9 = Secret Star-Gate unlocked (auto-set when flags 6+7+8 active)
;  10 = Read the Forbidden Verse in Necronomicon (Madness Truth path)

[node log_01]
prompt_zh 调查：潮湿的日志残页 #1
prompt_en INSPECT: DAMP LOG PAGE #1
speaker_zh 无署名日志 #1（潮汐栈道）
speaker_en UNSIGNED LOG #1 (TIDAL PIER)
color 14
text_zh 十月十三日，子夜。百年一遇的日蚀尚未降临，黑礁周围的海水却已诡异退去三寻。
text_zh 封死北侧海沟的玄武海蚀门正在发出低沉轰鸣。不管谁读到这页纸——先去栈道尽头点燃信号火盆。
text_en Oct 13, midnight. Hours before the century eclipse, the sea around Blackreef drained three fathoms.
text_en The basalt sea-gate to the north is groaning. Whoever reads this—light the dock brazier at the pier's end.
mad_zh 纸页在滴水。墨迹扭曲成一行新字：“火盆不是用来警示船只的，你早就知道。”
mad_en The page drips black brine. The ink twists: "The brazier was never meant to warn ships. You already know."

[node log_02]
prompt_zh 调查：残破航海志 #2
prompt_en INSPECT: TORN LOG #2
speaker_zh 无署名日志 #2（礁石断崖）
speaker_en UNSIGNED LOG #2 (REEF CLIFF)
color 11
text_zh 退潮露出的不是暗礁，而是倾斜的非欧几里得石阶。绿色的星辰正在北方的天穹排成一列。
text_zh 沉没圣殿的三翼各有一枚旧印：西侧千眼回廊、东侧管风琴圣咏台、北侧黑星高台。
text_en What rose from the low tide is no reef—it is cyclopean stonework. Green stars align over the northern sky.
text_en Three Elder Seals guard the sunken city: the Western Eyes, the Eastern Choir, and the Northern Overlook.

[node log_secret]
prompt_zh 调查：藏在灯塔背面的锈蚀铁盒
prompt_en INSPECT: RUSTED IRON BOX BEHIND LIGHTHOUSE
speaker_zh 无署名残页 #0（锁在铁盒里的字条）
speaker_en UNSIGNED NOTE #0 (LOCKED IN IRON)
color 13
on_enter -5 7 0 0
text_zh 你撬开锈蚀铁盒，里面只有一张被盐水浸透、字迹颤抖的纸条，落款已被海水冲毁：
text_zh “我没有逃走。百年之约需要一个活着的提灯者跨入北翼星门，以自身取代旧印。若你在东翼听见残响，请宽恕那份怯懦。”
text_en Inside the rusted box lies a brine-soaked note, its signature washed away:
text_en "I did not flee. The century pact demands a living lantern-bearer step through the North Star-Gate. If you meet what lingers in the East Choir, forgive that cowardice."

[node brazier_dock]
prompt_zh 点燃：码头信号火盆
prompt_en IGNITE: DOCK SIGNAL BRAZIER
speaker_zh 码头信号火盆
speaker_en DOCK SIGNAL BRAZIER
color 10
on_enter 10 1 0 0
text_zh 你将提灯火星投入油槽，苍白火焰骤然化作幽绿！远处海底传来石柱断裂般的沉闷回响——
text_zh 北侧封死海沟的【海蚀巨门】正在缓缓沉入海底，露出了通往沉船海沟的阶梯。
text_en Your lantern sparks the basin—the flame flares eerie emerald! A deep grinding rumble shakes the reef—
text_en The northern SEA-GATE sinks into the seabed, revealing the wet steps down into the Wreckage Trench.

[node trench_tablet]
prompt_zh 调查：退潮海床石碑
prompt_en INSPECT: SEABED TABLET
speaker_zh 古代石碑（沉船海沟）
speaker_en ANCIENT TABLET (WRECKAGE TRENCH)
color 6
text_zh 刻满藤壶的石碑上留着几行新鲜凿痕：“跨过前方的白骨拱门，就再也没有回头路。”
text_zh “凝视黑日与沉眠者会侵蚀你的理智；靠近灯塔与旧印火盆可暂保清醒。”
text_en Fresh chisel marks cut through barnacles: "Beyond the bone arch ahead, the surface world ceases to exist."
text_en "Gazing at the Black Sun drains sanity; linger near lit seals or the lighthouse to steady your mind."
mad_zh 石碑上的字正在蠕动：“别回头，黑日已经看见你的名字了。”
mad_en The carved runes writhe: "Do not turn back. The Black Sun already knows your name."

[node enter_rlyeh]
prompt_zh 踏入拉莱耶
prompt_en ENTER R'LYEH
speaker_zh 天象异变：拉莱耶沉没圣殿
speaker_en ECLIPSE AWAKENS: SUNKEN R'LYEH
color 9
on_enter -8 2 0 0
text_zh 踏过拱门的瞬间，头顶宁静的星海骤然扭曲——一轮燃烧着绿红日冕的【黑日蚀月】在北方天穹展开！
text_zh 利维坦脊骨桥在黑海中起伏呼吸，三翼的旧印正等待重燃。顺着罗盘信标探索西、东、北三翼吧。
text_en The moment you cross the arch, the quiet stars fracture—a colossal BLACK SUN ECLIPSE ignites in the north!
text_en The Leviathan Spine breathes over the black sea. Follow your compass to rekindle the West, East, and North Seals.

[node necronomicon]
prompt_zh 调查：悬浮的《死灵之书》
prompt_en INSPECT: HOVERING NECRONOMICON
speaker_zh 《死灵之书》（西翼·血肉之心）
speaker_en THE NECRONOMICON (WEST: BLEEDING HEART)
color 13
text_zh 湿冷的皮革书页在半空无风自翻，书脊渗出暗红荧光。西之封印正被这本禁忌典籍所压制。
text_zh 书页上浮现出一段直刺脑海的星辰颂歌——你要怎么做？
text_en Salt-blackened pages turn by themselves in mid-air, suppressing the Western Elder Seal beneath crimson ink.
text_en A forbidden star-verse burns into your vision. What will you do?
mad_zh 书页在欢呼你的到来。它承诺只要念出最后一行，你就能听懂深海的低语。
mad_en The pages rejoice at your arrival, promising you will finally understand the song beneath the waves.
choice seal_west_done 15 3 0 0 以守灯人的火焰焚毁禁忌残页（+理智，点燃西之封印） | Burn the forbidden page with your lantern (+SAN, Light West Seal)
choice seal_west_mad -35 3 10 0 诵读禁忌星文，窥探深渊真相（-大量理智，点燃西之封印） | Read the sunken verse aloud (-SAN, Gain Forbidden Truth & Light Seal)

[node seal_west_done]
speaker_zh 西之封印已重燃
speaker_en WEST SEAL REKINDLED
color 10
text_zh 翠绿的旧印火焰吞没了禁忌书页，压迫感骤然消散。海面轰鸣——西侧通往中央圣殿的【捷径石桥】已升出水面！
text_en Emerald flame consumes the cursed page. Across the dark water, the WESTERN SHORTCUT BRIDGE rises from the sea!

[node seal_west_mad]
speaker_zh 禁忌真相（西之封印已燃）
speaker_en FORBIDDEN TRUTH (WEST SEAL LIT)
color 9
text_zh 剧痛贯穿颅骨！你看见了百年之前建造黑礁灯塔的真正蓝图——那并非警示航标，而是指向拉莱耶的引路星灯！
text_zh 西侧捷径桥轰然升起，而黑日的竖瞳似乎正微微转向你。
text_en Agony tears through your mind! You glimpse Blackreef's true blueprint—it was never a warning beacon, it was a summoning flare!
text_en The Western Shortcut Bridge rises, and the Black Sun's slit eye turns subtly toward you.

[node echo_east]
prompt_zh 对话：苍白的提灯残影
prompt_en COMMUNE: PALE LANTERN ECHO
speaker_zh 苍白提灯残影（东翼·溺亡圣咏台）
speaker_en PALE LANTERN ECHO (EAST: DROWNED CHOIR)
color 14
text_zh 雷电劈落管风琴巨柱，一道穿着浸水油布大衣、提着幽蓝古灯的半透明身影在星芒碑前转身望向你。
text_zh “果然又有人下来了……东之封印的火种在我手里。那年我没敢踏入北翼的星门，才害得潮汐提前退去。”
text_en Lightning strikes the organ spires as a translucent figure in a brine-soaked oilskin coat turns with a cyan lantern.
text_en "Another one came down after all... I hold the East Seal's spark. I lacked the courage to enter the North Star-Gate."
choice echo_taken -5 4 0 0 夺取幽蓝古灯，独自承担封印（点燃东之封印） | Claim the cyan lantern and seal the abyss alone (Light East Seal)
choice echo_forgiven 10 4 8 0 宽恕那份怯懦，接过守望的火种（点燃东之封印） | Forgive that fear and accept the vigil (Light East Seal)

[node echo_taken]
speaker_zh 东之封印已重燃
speaker_en EAST SEAL REKINDLED
color 14
text_zh 残影在雷光中悄然碎裂，幽蓝星火汇入东翼星芒碑。东侧直通中央祭坛的【捷径石桥】破浪升起！
text_en The echo shatters in a flash of cyan lightning. The EASTERN SHORTCUT BRIDGE surges up from the waves!

[node echo_forgiven]
speaker_zh 残影的嘱托（东之封印已燃）
speaker_en THE ECHO'S BLESSING (EAST SEAL LIT)
color 10
text_zh “谢谢你……”身影化作一缕银色星尘融入星芒碑，“若你在灯塔后读过那张无署名的字条，三印齐燃时，北翼的星门会为你敞开。”
text_zh 东侧通往中央祭坛的【捷径石桥】轰然升起！
text_en "Thank you..." The figure dissolves into silver stardust. "If you read that unsigned note behind the lighthouse, the North Star-Gate will open once all three seals burn."
text_en The EASTERN SHORTCUT BRIDGE surges up from the waves!

[node seal_north]
prompt_zh 点燃：北翼黑星火盆
prompt_en IGNITE: NORTH BLACK-STAR BRAZIER
speaker_zh 北之封印（黑星高台）
speaker_en NORTH SEAL (BLACK-STAR OVERLOOK)
color 11
on_enter 10 5 0 0
text_zh 你在整座沉没之城的最高处点燃了北之封印。回望南方，拉莱耶圣殿、雷电管风琴与极远处的黑礁灯塔尽收眼底。
text_zh 若三翼封印均已重燃，中央祭坛前的封印屏障将会沉降——是时候做出最终抉择了。
text_en High above the sunken city, the Northern Seal ignites. Looking south, all of R'lyeh—and far Blackreef Lighthouse—stretches beneath the stars.
text_en Once all three seals burn, the barrier before the Central Altar will sink. Return to face the Sleeper.

[node final_altar]
prompt_zh 仪式：直面沉眠者祭坛
prompt_en RITUAL: CONFRONT THE SLEEPER'S ALTAR
speaker_zh 拉莱耶中央祭坛
speaker_en CENTRAL ALTAR OF R'LYEH
color 9
text_zh 三道光柱自西、东、北三翼汇聚到克苏鲁巨像脚下，古老的星辰颂歌震荡着整片黑海。
text_zh 你的提灯在手中剧烈颤抖——你要将祂重新封入深海，还是熄灭灯火、见证黑星降临？
text_en Three beams of light converge at the foot of the awakening colossus as the black sea trembles.
text_en Your lantern shudders in your hand. Will you bind Him beneath the waves, or extinguish the light?
mad_zh 你的心跳与沉眠者的呼吸完全同步。为什么还要抵抗？熄灭那盏可笑的小灯，抬头迎接黑日吧。
mad_en Your heartbeat matches the Sleeper's breath. Why resist? Snuff out that pitiful lantern and behold the Black Sun.
choice end 20 0 0 1 高举守灯人之火，重铸百年封印（结局 A） | Raise the Keeper's Flame and restore the Seal (Ending A)
choice end -50 0 0 2 熄灭提灯，向黑日献上见证（结局 B） | Extinguish the lantern and witness the Awakening (Ending B)

[node star_gate]
prompt_zh 踏入：觉醒的北翼星门
prompt_en ENTER: AWAKENED NORTH STAR-GATE
speaker_zh 维度星门（北翼深渊）
speaker_en THE STAR-GATE (NORTHERN ABYSS)
color 13
text_zh 紫青色的星辉漩涡在你面前旋转。你已将铁盒字条与那道提灯残影的秘密拼凑完整。
text_zh 只要踏出这一步，沉眠之城将彻底安息，而你将成为新的永恒守望者。
text_en A violet-cyan vortex of starlight turns before you. The unsigned note and the pale lantern echo finally fit together.
text_en One step forward will lull R'lyeh to eternal rest—at the cost of taking that vigil beyond the stars.
choice end 30 0 0 3 踏入星门，接替永恒守望（隐藏结局 C） | Step into the Star-Gate and take the vigil (Secret Ending C)
choice end 0 0 0 0 暂不踏入，返回圣殿 | Step back for now
