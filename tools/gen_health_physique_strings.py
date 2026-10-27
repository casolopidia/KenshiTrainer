# -*- coding: utf-8 -*-
"""Generate medical health/physique UI strings as v100-safe ASCII."""
from pathlib import Path
import re

STRINGS = [
    ('PART_HEAD', '头部', 'Head'),
    ('PART_CHEST', '胸部', 'Chest'),
    ('PART_STOMACH', '腹部', 'Stomach'),
    ('LBL_HEALTH_EDITOR', '全身血液与部位血量', 'Blood and body part health'),
    ('LBL_BLOOD', '全身血液：', 'Total blood:'),
    ('HINT_BLOOD', '全身血液独立于各部位血量。输入目标血液后点击「设定」，允许范围为负上限至正上限；「回满」只恢复血液，不治疗部位伤势。', 'Total blood is independent of body part HP. Set accepts values from -Max to +Max. Full restores blood only and does not heal body parts.'),
    ('MSG_BLOOD_OK', '全身血液设置已应用', 'Total blood setting applied'),
    ('MSG_BLOOD_FAIL', '血液设置失败：目标值须在负上限至正上限之间', 'Blood edit failed: target must be between -Max and +Max'),
    ('LBL_PHYSIQUE_EDITOR', '形体调节与保持', 'Physique adjustment and lock'),
    ('LBL_PROSTHETIC_EDITOR', '截肢与义肢', 'Amputation and prosthetics'),
    ('LBL_BODY_PART', '部位', 'Body part'),
    ('LBL_CURRENT_MAX', '当前 / 上限', 'Current / Max'),
    ('LBL_TARGET_HP', '目标血量', 'Target HP'),
    ('LBL_TARGET_MAX', '目标上限', 'Target max'),
    ('LBL_ACTION', '操作', 'Action'),
    ('BTN_HEALTH_FULL', '回满', 'Full'),
    ('HINT_PART_HEALTH', '可修改当前选中角色的部位血量与实际上限（含已安装义肢）。编辑两个目标值后点击「设定」；当前血量允许范围为负上限至正上限，上限须大于 0 且不超过 1000000。上限按种族、年龄倍率及义肢磨损换算，保留这些原有影响。「回满」仅按当前真实上限恢复，不修改上限或全身血液。', 'Edit current HP and actual max HP for the selected character, including prosthetics. Set applies both target fields. Current HP ranges from -Max to +Max; max must be greater than 0 and at most 1000000. Max is converted through race/age multipliers and prosthetic wear, preserving their effects. Full only heals to the current cap and changes neither max HP nor total blood.'),
    ('MSG_HEALTH_OK', '部位血量设置已应用', 'Body part health settings applied'),
    ('MSG_HEALTH_FAIL', '设置失败：检查血量范围、上限及部位状态', 'Edit failed: check HP range, maximum and body part state'),
    ('MSG_BAD_MAX_VALUE', '上限须大于 0 且不超过 1000000', 'Maximum must be greater than 0 and at most 1000000'),
    ('MSG_HEALTH_UNAVAILABLE', '暂时无法读取角色医疗数据', 'Character medical data is unavailable'),
    ('LBL_PHYSIQUE_CURRENT', '当前体格 / 肌肉 / 消瘦：', 'Current bulk / muscle / skinny:'),
    ('LBL_PHYSIQUE_LOCK', '保持外形', 'Lock physique'),
    ('LBL_PHYSIQUE_BULK', '体格（0～100）', 'Bulk (0-100)'),
    ('LBL_PHYSIQUE_MUSCLE', '肌肉（0～100）', 'Muscle (0-100)'),
    ('LBL_PHYSIQUE_SKINNY', '消瘦（0～100）', 'Skinny (0-100)'),
    ('BTN_CAPTURE_PHYSIQUE', '锁定当前外形', 'Freeze current physique'),
    ('BTN_APPLY_PHYSIQUE', '应用数值并保持', 'Apply values and lock'),
    ('HINT_PHYSIQUE', '只调节由属性与营养驱动的外观，不修改力量、敏捷、技能或饥饿值。体格影响粗壮程度，肌肉影响肌肉外观，消瘦值越高越瘦。拖动滑条（Ctrl+点击可输入数值），再点击「应用数值并保持」；取消「保持外形」恢复游戏自动变化。', 'Changes only the appearance driven by stats and nutrition. Strength, dexterity, skills and hunger stay unchanged. Bulk controls body build; muscle controls muscular appearance; higher skinny means thinner. Drag sliders (Ctrl+click to type), then Apply values and lock. Uncheck Lock physique to restore automatic changes.'),
    ('HINT_PHYSIQUE_SESSION', '锁定后，力量、相关技能或营养状态改变时仍保持这三个外观倍率，不会冻结属性本身。按角色独立保持，关闭面板或切换角色仍生效；角色卸载或读档后需重新设置。下次游戏外观更新时生效，暂停时需恢复运行。模型加载期间也可先设置；动物不支持，实际效果取决于人形种族模型。', 'While locked, the three appearance multipliers stay fixed when strength, related skills or nutrition change; the stats themselves are not frozen. Locks are per character and remain active with the panel closed or another character selected. Reapply after character unload or loading a save. Changes take effect on the next game appearance update; resume if paused. Values can be queued while the model loads. Animals are unsupported; results depend on the humanoid race model.'),
    ('MSG_PHYSIQUE_QUEUED', '外形设置已提交，将在下次游戏外观更新时生效', 'Physique change queued for the next game appearance update'),
    ('MSG_PHYSIQUE_FAIL', '外形设置失败：请确认角色模型已加载且数值有效', 'Physique edit failed: check that the model is loaded and values are valid'),
    ('MSG_PHYSIQUE_UNAVAILABLE', '形体功能未启用，请检查 RE_Kenshi_log.txt 的 physique hooks 日志及 KT_NOPHYSIQUE 环境变量。', 'Physique control unavailable: check physique hooks in RE_Kenshi_log.txt and the KT_NOPHYSIQUE environment variable.'),
    ('MSG_PHYSIQUE_UNSUPPORTED', '当前角色没有可读取的人形外观数据，请选择人形角色后重试。', 'No readable humanoid appearance data; select a humanoid character and retry.'),
    ('HINT_MEDICAL', '提示：可直接安装义肢，无需先截肢；替换时旧义肢会放回背包。每个部位只显示明确匹配的型号，无法识别部位的模组义肢会被拒绝。', 'Note: install directly without amputation; replaced prosthetics return to the backpack. Each slot lists only positively matched models. Modded limbs with unknown compatibility are rejected.'),
    ('MSG_NO_COMPATIBLE_LIMB', '没有已识别为兼容此部位的义肢', 'No prosthetic positively identified for this limb'),
    ('MSG_LIMB_INCOMPATIBLE', '安装已阻止：该义肢与目标部位不匹配或无法识别', 'Install blocked: prosthetic does not match the target limb or is unrecognized'),
]

path = Path(__file__).resolve().parents[1] / 'src/ChineseStrings.h'
text = path.read_text(encoding='ascii')
for key, cn, en in STRINGS:
    for language, value in (('CN', cn), ('EN', en)):
        name = key + '_' + language
        escaped = ''.join('\\x%02x' % b for b in value.encode('utf-8'))
        line = '    static const char* %s = "%s";' % (name, escaped)
        pattern = r'    static const char\* ' + name + r' = "[^"\n]*";'
        if re.search(pattern, text):
            text = re.sub(pattern, lambda _: line, text)
        else:
            marker = '}\n#define TR(k)'
            assert marker in text
            text = text.replace(marker, line + '\n' + marker)
path.write_text(text, encoding='ascii', newline='\n')
print('Updated %d CN/EN health and physique strings' % len(STRINGS))
