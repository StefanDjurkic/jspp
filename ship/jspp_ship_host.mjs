#!/usr/bin/env node

import readline from "readline";
import { parseSource, Interpreter } from "../prototype/jspp.mjs";
import { TypeChecker } from "../prototype/typechecker.mjs";

const TYPECHECK_PRELUDE = [
  "let console = { log: print };",
  "function drawRect(x, y, width, height, r, g, b) {}",
  "function drawCircle(x, y, radius, r, g, b) {}",
  "function drawLine(x1, y1, x2, y2, r, g, b) {}",
  "function clear(r, g, b) {}",
  "",
].join("\n");

function clampColor(value, fallback = 0) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) {
    return fallback;
  }
  return Math.max(0, Math.min(255, Math.round(numeric)));
}

function toNumber(value, fallback = 0) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

function makeDefaultScene() {
  return {
    clear: { r: 18, g: 18, b: 22 },
    shapes: [],
  };
}

function encodeLine(value) {
  return Buffer.from(String(value), "utf8").toString("base64");
}

function decodeLine(value) {
  return Buffer.from(value, "base64").toString("utf8");
}

function splitOutputLines(line) {
  return String(line)
    .replace(/\r/g, "")
    .split("\n")
    .filter(part => part.length > 0);
}

class ShipHostSession {
  constructor() {
    this.reset();
  }

  reset() {
    this.history = [];
    this.scene = makeDefaultScene();
    this.output = [];
    this.interpreter = new Interpreter({
      onOutput: line => this.captureOutput(line),
    });
    this.installBuiltins();
  }

  captureOutput(line) {
    this.output.push(...splitOutputLines(line));
  }

  installBuiltins() {
    const consoleObject = {
      log: (...args) => {
        const line = args.map(arg => String(arg)).join(" ");
        this.captureOutput(line);
      },
    };
    this.interpreter.global.define("console", consoleObject);
    this.interpreter.global.define("drawRect", (x, y, width, height, r, g, b) => {
      this.scene.shapes.push({
        kind: "RECT",
        x: toNumber(x),
        y: toNumber(y),
        width: toNumber(width),
        height: toNumber(height),
        r: clampColor(r, 255),
        g: clampColor(g, 255),
        b: clampColor(b, 255),
      });
    });
    this.interpreter.global.define("drawCircle", (x, y, radius, r, g, b) => {
      this.scene.shapes.push({
        kind: "CIRCLE",
        x: toNumber(x),
        y: toNumber(y),
        radius: toNumber(radius),
        r: clampColor(r, 255),
        g: clampColor(g, 255),
        b: clampColor(b, 255),
      });
    });
    this.interpreter.global.define("drawLine", (x1, y1, x2, y2, r, g, b) => {
      this.scene.shapes.push({
        kind: "LINE",
        x1: toNumber(x1),
        y1: toNumber(y1),
        x2: toNumber(x2),
        y2: toNumber(y2),
        r: clampColor(r, 255),
        g: clampColor(g, 255),
        b: clampColor(b, 255),
      });
    });
    this.interpreter.global.define("clear", (r = this.scene.clear.r, g = this.scene.clear.g, b = this.scene.clear.b) => {
      this.scene.clear = {
        r: clampColor(r, this.scene.clear.r),
        g: clampColor(g, this.scene.clear.g),
        b: clampColor(b, this.scene.clear.b),
      };
      this.scene.shapes = [];
    });
  }

  typeCheck(source) {
    const fullSource = TYPECHECK_PRELUDE + this.history.join("\n") + (this.history.length > 0 ? "\n" : "") + source;
    const { ast } = parseSource(fullSource, "<ship-session>");
    const checker = new TypeChecker();
    return checker.check(ast);
  }

  execute(source) {
    const response = {
      ok: false,
      scene: this.scene,
      output: [],
      infos: [],
      errors: [],
    };
    try {
      const diagnostics = this.typeCheck(source);
      for (const diagnostic of diagnostics) {
        if (diagnostic.severity === "error") {
          response.errors.push(String(diagnostic));
        } else {
          response.infos.push(String(diagnostic));
        }
      }
      if (response.errors.length > 0) {
        response.scene = this.scene;
        return response;
      }

      this.output = [];
      this.interpreter.output = [];
      const { ast } = parseSource(source, "<ship-input>");
      this.interpreter.run(ast);
      this.history.push(source);

      response.ok = true;
      response.scene = this.scene;
      response.output = [...this.output];
      return response;
    } catch (error) {
      response.scene = this.scene;
      response.errors.push(String(error instanceof Error ? error.message : error));
      return response;
    }
  }
}

function writeResponse(response) {
  process.stdout.write(`RESULT ${response.ok ? "OK" : "ERROR"}\n`);
  process.stdout.write(`CLEAR ${response.scene.clear.r} ${response.scene.clear.g} ${response.scene.clear.b}\n`);
  process.stdout.write(`SHAPES ${response.scene.shapes.length}\n`);
  for (const shape of response.scene.shapes) {
    if (shape.kind === "RECT") {
      process.stdout.write(`RECT ${shape.x} ${shape.y} ${shape.width} ${shape.height} ${shape.r} ${shape.g} ${shape.b}\n`);
    } else if (shape.kind === "CIRCLE") {
      process.stdout.write(`CIRCLE ${shape.x} ${shape.y} ${shape.radius} ${shape.r} ${shape.g} ${shape.b}\n`);
    } else if (shape.kind === "LINE") {
      process.stdout.write(`LINE ${shape.x1} ${shape.y1} ${shape.x2} ${shape.y2} ${shape.r} ${shape.g} ${shape.b}\n`);
    }
  }
  process.stdout.write(`OUTPUT ${response.output.length}\n`);
  for (const line of response.output) {
    process.stdout.write(`${encodeLine(line)}\n`);
  }
  process.stdout.write(`INFOS ${response.infos.length}\n`);
  for (const line of response.infos) {
    process.stdout.write(`${encodeLine(line)}\n`);
  }
  process.stdout.write(`ERRORS ${response.errors.length}\n`);
  for (const line of response.errors) {
    process.stdout.write(`${encodeLine(line)}\n`);
  }
  process.stdout.write("END\n");
}

const session = new ShipHostSession();
const rl = readline.createInterface({
  input: process.stdin,
  crlfDelay: Infinity,
  terminal: false,
});

rl.on("line", line => {
  if (!line) {
    return;
  }
  if (line === "RESET") {
    session.reset();
    writeResponse({ ok: true, scene: session.scene, output: [], infos: [], errors: [] });
    return;
  }
  if (line === "QUIT") {
    rl.close();
    process.exit(0);
  }
  if (!line.startsWith("EXEC ")) {
    writeResponse({ ok: false, scene: session.scene, output: [], infos: [], errors: [`Unknown command: ${line}`] });
    return;
  }

  const source = decodeLine(line.slice(5));
  writeResponse(session.execute(source));
});

rl.on("close", () => {
  process.exit(0);
});