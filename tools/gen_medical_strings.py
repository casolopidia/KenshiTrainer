# -*- coding: utf-8 -*-
# Appends medical-tab strings to ChineseStrings.h (idempotent, pure ASCII out).
import io, sys

HDR = r"src\ChineseStrings.h"

NEW = [
    ("TAB_MEDICAL", "医疗", "Medical"),
    ("LIMB_LEFT_ARM", "左臂", "Left arm"),
    ("LIMB_RIGHT_ARM", "右臂", "Right arm"),
    ("LIMB_LEFT_LEG", "左腿", "Left leg"),
    ("LIMB_RIGHT_LEG", "右腿", "Right leg"),
    ("STATE_ORIGINAL", "原生", "Original"),
    ("STATE_STUMP", "断肢", "Severed"),
    ("STATE_REPLACED", "义肢", "Prosthetic"),
    ("STATE_CRUSHED", "粉碎", "Crushed"),
    ("BTN_AMPUTATE", "截肢", "Sever"),
    ("BTN_ATTACH", "安装", "Attach"),
    ("BTN_RESTORE", "恢复原生", "Restore"),
    ("LBL_PROSTHETIC", "义肢型号:", "Prosthetic:"),
    ("MSG_LIMB_SEVERED", "已截肢:", "Severed:"),
    ("MSG_LIMB_ATTACHED", "义肢已安装:", "Attached:"),
    ("MSG_LIMB_RESTORED", "原生肢体已恢复:", "Restored:"),
    ("MSG_LIMB_FAIL", "肢体操作失败", "Limb operation failed"),
    ("MSG_NO_LIMB_ITEMS", "未找到义肢数据", "No prosthetic items found"),
    ("HINT_MEDICAL", "提示: 可直接安装义肢，无需先截肢；替换时旧义肢会放回背包", "Note: install directly without amputation; replaced prosthetics return to the backpack"),
]

def esc(s):
    return "".join("\\x%02x" % b for b in s.encode("utf-8"))

with io.open(HDR, "r", encoding="ascii") as f:
    text = f.read()

added = []
block_lines = []
for key, cn, en in NEW:
    if (key + "_CN") in text:
        continue
    block_lines.append('    static const char* %s_CN = "%s";' % (key, esc(cn)))
    block_lines.append('    static const char* %s_EN = "%s";' % (key, esc(en)))
    added.append(key)

if not block_lines:
    print("nothing to add")
    sys.exit(0)

marker = "}\n#define TR(k)"
idx = text.rfind(marker)
assert idx > 0, "TR macro marker not found"
block = "\n".join(block_lines) + "\n"
text = text[:idx] + block + text[idx:]

with io.open(HDR, "w", encoding="ascii", newline="\n") as f:
    f.write(text)
print("added %d keys: %s" % (len(added), ", ".join(added)))