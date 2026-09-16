# -*- coding: utf-8 -*-
import pathlib, time, json
from playwright.sync_api import sync_playwright

out = pathlib.Path(r"D:\kenshi内置修改器mod\tools\wiki_fetch")

PAGES = [
 ("开局指南","开局指南"),("游戏地图","游戏地图"),("人物训练","人物训练"),("如何赚钱","如何赚钱"),
 ("保持健康","保持健康"),("游戏机制","游戏机制"),("种族","种族"),("个性","个性"),
 ("角色数据","角色数据"),("动物","动物"),("特殊招募","特殊招募"),("悬赏犯","悬赏犯"),
 ("派系关系","派系关系"),("法律体系","法律体系"),("悬赏机制","悬赏机制"),("贸易文化","贸易文化"),
 ("事件","事件"),("环境","环境"),("季节","季节"),("天气效果","天气效果"),
 ("世界状态","世界状态"),("勘探","勘探"),("城镇覆盖","城镇覆盖"),("主要城镇","主要城镇"),
 ("巢穴","巢穴"),("各古代实验室","各古代实验室"),("建筑","建筑"),("建立据点","建立据点"),
 ("科技树","科技树"),("铭物","铭物"),("武士刀","武士刀"),("军刀","军刀"),
 ("砍刀","砍刀"),("重型武器","重型武器"),("钝器","钝器"),("长柄刀","长柄刀"),
 ("弩","弩"),("头盔","头盔"),("铠甲","铠甲"),("裤子","裤子"),
 ("衬衫","衬衫"),("鞋子","鞋子"),("假肢","假肢"),("背包","背包"),
 ("科研道具","科研道具"),("食物","食物"),("医疗用品","医疗用品"),("蓝图","蓝图"),
 ("贸易商品","贸易商品"),("特殊角色","特殊角色"),("外交角色","外交角色"),
]

def enc(t): 
    from urllib.parse import quote
    return quote(t)

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=r"C:\Users\Administrator\AppData\Local\ms-playwright\chromium-1228\chrome-win64\chrome.exe", headless=True, args=["--disable-blink-features=AutomationControlled"])
    ctx = browser.new_context(
        user_agent="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36",
        locale="zh-CN", viewport={"width":1366,"height":900})
    page = ctx.newPage if False else ctx.new_page()
    # warm up + get CF cookie
    page.goto("https://kenshi.huijiwiki.com/wiki/" + enc("首页"), wait_until="domcontentloaded", timeout=60000)
    page.wait_for_timeout(6000)
    results = {}
    for name, _ in PAGES:
        try:
            page.goto("https://kenshi.huijiwiki.com/wiki/" + enc(name), wait_until="domcontentloaded", timeout=60000)
            page.wait_for_timeout(1500)
            title = page.title()
            if "验证" in title or title.strip()=="":
                page.wait_for_timeout(8000)
                title = page.title()
            txt = page.locator("#mw-content-text").inner_text()
            (out / (name + ".txt")).write_text(txt, encoding="utf-8")
            results[name] = len(txt)
            print(name, len(txt), title[:30], flush=True)
        except Exception as e:
            results[name] = "ERR " + str(e)[:80]
            print(name, "ERR", str(e)[:80], flush=True)
        time.sleep(1.2)
    browser.close()
print(json.dumps(results, ensure_ascii=False))
