# -*- coding: utf-8 -*-
"""Add/update XP UI strings without hand-editing v100 UTF-8 hex escapes."""
from pathlib import Path
import re

STRINGS = [
    ("TAB_XP", "经验倍率", "XP Multiplier"),
    ("LBL_XP_MULTIPLIER", "经验倍率", "XP multiplier"),
    ("LBL_XP_SCOPE", "生效范围（可同时勾选）：", "Apply to (select either or both):"),
    ("LBL_XP_PLAYER", "玩家派系", "Player faction"),
    ("LBL_XP_AI", "AI 派系（所有非玩家派系）", "AI factions (all non-player factions)"),
    ("BTN_XP_RESET", "恢复 1 倍", "Reset to 1x"),
    ("HINT_XP", "调整今后获得的技能经验，设置自动保存。支持 0～1000 倍；1 倍为原速，0 倍停止所选范围的经验增长。均不勾选时保持原速。", "Changes future skill XP; settings save automatically. Range: 0-1000x. 1x is normal; 0x stops XP for the selected factions. Neither selected means normal XP."),
    ("HINT_XP_AI", "按角色所属派系判断，友好与敌对 AI 都属于 AI 范围。仅调整游戏原本会获得的经验；不会让原本不获经验的 AI 行为（如跑步）额外获得经验。训练条件与等级限制仍由游戏决定。", "Scope follows faction ownership, including friendly and hostile AI. Only existing XP gains are scaled; AI actions that normally grant no XP (such as running) still grant none. Training conditions and level limits remain controlled by the game."),
    ("XP_UNAVAILABLE", "经验倍率未启用：请检查 RE_Kenshi_log.txt 中的 XP hook 日志及 KT_NOXP 环境变量。", "XP multiplier unavailable: check XP hook messages in RE_Kenshi_log.txt and the KT_NOXP environment variable."),
]

path = Path(__file__).resolve().parents[1] / "src/ChineseStrings.h"
text = path.read_text(encoding="ascii")
for key, cn, en in STRINGS:
    for language, value in (("CN", cn), ("EN", en)):
        name = key + "_" + language
        escaped = "".join("\\x%02x" % b for b in value.encode("utf-8"))
        line = '    static const char* %s = "%s";' % (name, escaped)
        pattern = r'    static const char\* ' + name + r' = "[^"\n]*";'
        if re.search(pattern, text):
            text = re.sub(pattern, lambda _: line, text)
        else:
            marker = "}\n#define TR(k)"
            assert marker in text
            text = text.replace(marker, line + "\n" + marker)
path.write_text(text, encoding="ascii", newline="\n")
print("Updated %d CN/EN XP strings" % len(STRINGS))
