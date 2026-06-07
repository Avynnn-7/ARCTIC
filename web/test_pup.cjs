const puppeteer = require('puppeteer');
(async () => {
  const browser = await puppeteer.launch();
  const page = await browser.newPage();
  const logs = [];
  const errors = [];
  page.on('console', msg => {
    const text = msg.text();
    logs.push(text);
    if (text.includes('Error') || text.includes('error') || text.includes('fail')) {
      errors.push(text);
    }
  });
  page.on('pageerror', err => errors.push('PAGE_ERROR: ' + err.toString()));
  
  await page.goto('http://localhost:5173', { waitUntil: 'networkidle0', timeout: 15000 });
  await new Promise(r => setTimeout(r, 5000));
  
  console.log('=== ALL LOGS ===');
  logs.forEach(l => console.log('  ' + l));
  console.log('=== ERRORS ===');
  errors.forEach(e => console.log('  ' + e));
  console.log('=== ERRORS COUNT: ' + errors.length + ' ===');
  
  await browser.close();
})();
