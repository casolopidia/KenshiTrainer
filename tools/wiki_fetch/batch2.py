# -*- coding: utf-8 -*-
import pathlib, time, json
from urllib.parse import quote
from playwright.sync_api import sync_playwright

out = pathlib.Path(r"D:\kenshi内置修改器mod\tools\wiki_fetch")
PAGES = ["地图","训练","机制","悬赏","属性","战斗","潜行","偷窃","奴隶","招募","开局","伤害","盔甲","护甲","研究","农业","采矿","烹饪","锻造","骨人","蜂巢族","沙克族","人类","绿原之子","焦土之子","苍白人","王子","工蜂","兵蜂","斯昆镇","枢纽城","恒","雾岛","蒙格勒","商人边缘地","斯坦沙漠","铁之谷","灰烬之地","复仇之谷","偏远之地","黑暗之指","骨人平原","沙漠","闪","沼泽地","南蜂巢","死寂之地","哀矿镇","眼窝镇","水泡山丘","白鼬镇","打渔村庄","蜂巢族之村","摸蛋镇","走私犯之村","小偷镇","烂牙镇","奴隶市场","特拉","圣国","联合城","沙克王国","浪人","雾人","食人族","虫之主","卡特龙","疯狂皮埃尔","天狗","菲尼克斯","泡泡山丘","泡儿","亚穆尔","濑户","雷亚","猫","迪格那","康","伊予","杰格尔","杰格","骨犬","山羊","河豚","牛","喙嘴兽","剪嘴鸥","蜘蛛"," Skin"]
PAGES = [p.strip() for p in PAGES]

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=r"C:\Users\Administrator\AppData\Local\ms-playwright\chromium-1228\chrome-win64\chrome.exe", headless=True, args=["--disable-blink-features=AutomationControlled"])
    ctx = browser.new_context(user_agent="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36", locale="zh-CN", viewport={"width":1366,"height":900})
    page = ctx.new_page()
    page.goto("https://kenshi.huijiwiki.com/wiki/" + quote("首页"), wait_until="domcontentloaded", timeout=60000)
    page.wait_for_timeout(5000)
    for name in PAGES:
        try:
            page.goto("https://kenshi.huijiwiki.com/wiki/" + quote(name), wait_until="domcontentloaded", timeout=60000)
            page.wait_for_timeout(1200)
            title = page.title()
            if "验证" in title: page.wait_for_timeout(8000)
            txt = page.locator("#mw-content-text").inner_text()
            if "这个页面不存在" in txt[:200]:
                print(name, "MISSING", flush=True); continue
            (out / (name + ".txt")).write_text(txt, encoding="utf-8")
            print(name, len(txt), flush=True)
        except Exception as e:
            print(name, "ERR", str(e)[:60], flush=True)
        time.sleep(1.0)
    browser.close()
