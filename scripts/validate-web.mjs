import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const scripts = [
  "web/js/firebase-config.js",
  "web/js/shell.js",
  "web/js/dashboard.js",
  "web/js/history.js",
  "web/js/advanced.js",
  "web/js/settings.js"
];
const pages = ["web/index.html", "web/history.html", "web/advanced.html", "web/settings.html"];
const errors = [];

for (const script of scripts) {
  try {
    execFileSync(process.execPath, ["--check", resolve(root, script)], { stdio: "pipe" });
  } catch (error) {
    errors.push(`${script}: ${error.stderr?.toString() || error.message}`);
  }
}

for (const page of pages) {
  const absolute = resolve(root, page);
  const html = readFileSync(absolute, "utf8");
  const references = [...html.matchAll(/(?:src|href)=["']([^"']+)["']/g)].map(match => match[1]);
  for (const reference of references) {
    if (/^(?:https?:|#|data:|mailto:)/.test(reference)) continue;
    const target = resolve(dirname(absolute), reference.split(/[?#]/)[0]);
    if (!existsSync(target)) errors.push(`${page}: referensi hilang ${reference}`);
  }
  if (/login|register|sign[ -]?in/i.test(html)) {
    errors.push(`${page}: masih memuat istilah autentikasi pengguna`);
  }
}

if (errors.length) {
  console.error(errors.join("\n"));
  process.exit(1);
}
console.log(`VOLTIX web validation passed: ${scripts.length} scripts, ${pages.length} pages.`);
