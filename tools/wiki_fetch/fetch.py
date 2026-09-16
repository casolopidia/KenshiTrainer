import sys, time, pathlib
from playwright.sync_api import sync_playwright

out = pathlib.Path(r"D:\kenshi内置修改器mod\tools\wiki_fetch")
url = sys.argv[1]
name = sys.argv[2]

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=r"C:\Users\Administrator\AppData\Local\ms-playwright\chromium-1228\chrome-win64\chrome.exe", headless=True, args=["--disable-blink-features=AutomationControlled"])
    ctx = browser.new_context(
        user_agent="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36",
        locale="zh-CN", viewport={"width":1366,"height":900})
    page = ctx.new_page()
    page.goto(url, wait_until="domcontentloaded", timeout=60000)
    page.wait_for_timeout(8000)
    title = page.title()
    html = page.content()
    (out / (name + ".html")).write_text(html, encoding="utf-8")
    body = page.locator("body").inner_text()
    (out / (name + ".txt")).write_text(body, encoding="utf-8")
    print("TITLE:", title)
    print("LEN:", len(html), len(body))
    browser.close()

