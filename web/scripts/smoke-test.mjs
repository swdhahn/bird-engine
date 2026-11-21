// Opens the web build in headless Chrome (WebGL 2 through SwiftShader) and
// fails unless the engine reaches its frame loop without errors. Prints the
// page's console output and a base64 screenshot for reviewers who cannot
// fetch workflow artifacts.
//
//   node smoke-test.mjs http://localhost:8000/index.html
import puppeteer from 'puppeteer-core';

const url = process.argv[2];
const timeoutMs = Number(process.env.SMOKE_TIMEOUT_MS ?? 240000);
const chrome = process.env.CHROME_PATH ?? '/usr/bin/google-chrome';

const browser = await puppeteer.launch({
  executablePath: chrome,
  headless: true,
  // Software WebGL can hold the page's main thread for a long frame.
  protocolTimeout: 120000,
  args: [
    '--no-sandbox',
    '--use-angle=swiftshader',
    '--enable-unsafe-swiftshader',
    '--ignore-gpu-blocklist',
    '--window-size=1280,720',
  ],
});

const startedAt = Date.now();
let frames = 0;
let failure = null;
const page = await browser.newPage();
await page.setViewport({ width: 1280, height: 720 });
page.on('console', (message) => {
  const text = message.text();
  const seconds = ((Date.now() - startedAt) / 1000).toFixed(1);
  console.log(`[page ${message.type()} +${seconds}s] ${text}`);
  if (text.startsWith('FPS:')) frames++;
  if (text.includes('Fatal:') || text.includes('Aborted(')) failure = text;
});
page.on('pageerror', (error) => {
  console.log(`[page error] ${error.message}`);
  failure = error.message;
});

await page.goto(url, { waitUntil: 'load', timeout: 120000 });
const deadline = Date.now() + timeoutMs;
while (frames < 2 && !failure && Date.now() < deadline) {
  await new Promise((resolve) => setTimeout(resolve, 1000));
}

try {
  const screenshot = await page.screenshot({ type: 'png', encoding: 'base64' });
  console.log(`Screenshot: ${screenshot.length} base64 characters`);
  for (let i = 0; i < screenshot.length; i += 1000) {
    console.log(`SCREENSHOT_PNG_BASE64 ${screenshot.slice(i, i + 1000)}`);
  }
} catch (error) {
  console.log(`No screenshot: ${error.message}`);
}
await browser.close();

if (failure) {
  console.log(`::error::The engine stopped: ${failure}`);
  process.exit(1);
}
if (frames < 2) {
  console.log('::error::The engine never reached its frame loop');
  process.exit(1);
}
console.log(`Engine running (${frames} FPS reports)`);
