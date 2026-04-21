#!/usr/bin/env node
// ============================================================================
// JSPP Regression Test Runner
// Usage: node tests/run.mjs [--verbose] [filter]
//
// Scans tests/ for *.jspp files with matching *.expected files.
// Runs each through the interpreter, compares stdout to expected output.
// ============================================================================

import { readdirSync, readFileSync } from "fs";
import { join, dirname } from "path";
import { fileURLToPath } from "url";
import { execFileSync } from "child_process";

const __dirname = dirname(fileURLToPath(import.meta.url));
const rootDir = join(__dirname, "..");
const testsDir = __dirname;
const jsppPath = join(rootDir, "prototype", "jspp.mjs");

const args = process.argv.slice(2);
const verbose = args.includes("--verbose");
const filter = args.find(a => !a.startsWith("--")) || null;

// Discover test files
const files = readdirSync(testsDir)
  .filter(f => f.endsWith(".jspp"))
  .sort();

const tests = files.map(f => {
  const name = f.replace(/\.jspp$/, "");
  const expectedFile = f.replace(/\.jspp$/, ".expected");
  return { name, jsppFile: f, expectedFile };
}).filter(t => {
  // Check .expected exists
  try { readFileSync(join(testsDir, t.expectedFile), "utf-8"); return true; }
  catch { return false; }
}).filter(t => {
  if (!filter) return true;
  return t.name.includes(filter);
});

console.log(`\n  JSPP Regression Suite — ${tests.length} tests\n`);

let passed = 0;
let failed = 0;
const failures = [];

for (const t of tests) {
  const label = t.name.padEnd(30);
  const expected = readFileSync(join(testsDir, t.expectedFile), "utf-8").replace(/\r\n/g, "\n").trimEnd();

  let actual;
  try {
    const raw = execFileSync(process.execPath, [jsppPath, join(testsDir, t.jsppFile), "--run"], {
      cwd: rootDir,
      encoding: "utf-8",
      timeout: 10000,
      stdio: ["pipe", "pipe", "pipe"],
    });
    // Strip the [jspp] meta lines, keep only program output
    actual = raw
      .replace(/\r\n/g, "\n")
      .split("\n")
      .filter(line => !line.startsWith("[jspp]") && line !== "")
      .join("\n")
      .trimEnd();
  } catch (err) {
    actual = `ERROR: ${err.stderr || err.message}`.trimEnd();
  }

  if (actual === expected) {
    console.log(`  ✓  ${label} PASS`);
    passed++;
  } else {
    console.log(`  ✗  ${label} FAIL`);
    failures.push({ name: t.name, expected, actual });
    failed++;
  }
}

// Summary
console.log(`\n  ─────────────────────────────────────`);
console.log(`  ${passed} passed, ${failed} failed, ${tests.length} total`);

if (failures.length > 0) {
  console.log(`\n  FAILURES:\n`);
  for (const f of failures) {
    console.log(`  ── ${f.name} ──`);
    console.log(`  Expected:`);
    for (const line of f.expected.split("\n")) console.log(`    ${line}`);
    console.log(`  Actual:`);
    for (const line of f.actual.split("\n")) console.log(`    ${line}`);
    console.log();
  }
  process.exit(1);
} else {
  console.log(`\n  All tests passed.\n`);
  process.exit(0);
}
