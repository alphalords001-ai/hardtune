// Tests du moteur JavaScript (avant livraison de la démo navigateur).
// node demo/test-node.js

const Tune = require('./tune-core.js');

let failures = 0;
function check(ok, label, detail) {
  console.log(`  [${ok ? ' OK ' : 'FAIL'}] ${label.padEnd(50)} ${detail || ''}`);
  if (!ok) failures++;
}

const SR = 44100;

function sine(hz, n, amp = 0.5) {
  const v = new Float32Array(n);
  for (let i = 0; i < n; i++) v[i] = amp * Math.sin(2 * Math.PI * hz * i / SR);
  return v;
}

function saw(f0, n) {
  const v = new Float32Array(n);
  for (let h = 1; h <= 24; h++) {
    const f = f0 * h;
    if (f > SR * 0.45) break;
    const a = 0.4 / h;
    for (let i = 0; i < n; i++) v[i] += a * Math.sin(2 * Math.PI * f * i / SR);
  }
  return v;
}

// mesure de fréquence par balayage fin (Goertzel)
function measureFreq(x, start, len, expected, spanPct = 0.04) {
  const N = 200;
  const f0 = expected * (1 - spanPct);
  const df = expected * 2 * spanPct / N;
  const mags = new Float64Array(N + 1);
  for (let k = 0; k <= N; k++) {
    const f = f0 + df * k;
    const w = 2 * Math.PI * f / SR;
    const coef = 2 * Math.cos(w);
    let s1 = 0, s2 = 0;
    for (let i = 0; i < len; i++) {
      const s0 = x[start + i] + coef * s1 - s2;
      s2 = s1; s1 = s0;
    }
    mags[k] = s1 * s1 + s2 * s2 - coef * s1 * s2;
  }
  let best = 0;
  for (let k = 1; k <= N; k++) if (mags[k] > mags[best]) best = k;
  let shift = 0;
  if (best > 0 && best < N) {
    const a = mags[best - 1], b = mags[best], c = mags[best + 1];
    const den = a - 2 * b + c;
    if (Math.abs(den) > 1e-20) shift = 0.5 * (a - c) / den;
  }
  return f0 + df * (best + shift);
}

function corr(x, y, lag, start, end) {
  let num = 0, d1 = 0, d2 = 0;
  for (let i = start; i < end; i++) {
    const a = x[i - lag], b = y[i];
    num += a * b; d1 += a * a; d2 += b * b;
  }
  return num / Math.sqrt(Math.max(1e-20, d1 * d2));
}

const baseOpts = { key: 0, scale: 0, customMask: 0, amount: 1, retuneMs: 0, transpose: 0, formant: 0 };

console.log('=== Tests du moteur JS (demo navigateur) ===\n');

// --- 1. detection ---------------------------------------------------------
console.log('TEST 1 — detection');
{
  [110, 220, 440, 880].forEach(f => {
    const x = sine(f, SR * 2);
    const frames = Tune.detect(x, SR, 512, 1024);
    const valid = frames.filter(fr => fr.hz > 0);
    const avg = valid.reduce((s, fr) => s + fr.hz, 0) / valid.length;
    const cents = Math.abs(Tune.ratioToCents(avg / f));
    check(cents < 10, `detection ${f} Hz`, `mesure ${avg.toFixed(1)} Hz (${cents.toFixed(2)} cents)`);
  });
}

// --- 2. correction de hauteur --------------------------------------------
console.log('\nTEST 2 — correction');
{
  const fIn = 227.0;                    // cible chromatique : 233.08
  const fTarget = Tune.midiToHz(Math.round(Tune.hzToMidi(fIn)));
  const x = sine(fIn, SR * 3);
  const r = Tune.process(x, SR, baseOpts);
  const fOut = measureFreq(r.out, SR, SR, fTarget);
  const cents = Math.abs(Tune.ratioToCents(fOut / fTarget));
  check(cents < 15, 'sinus desaccorde 227 Hz', `sortie ${fOut.toFixed(2)} Hz (cible ${fTarget.toFixed(2)}, ecart ${cents.toFixed(1)} cents)`);
}
{
  const fIn = 148.0;
  const fTarget = Tune.midiToHz(Math.round(Tune.hzToMidi(fIn)));
  const x = saw(fIn, SR * 3);
  const r = Tune.process(x, SR, baseOpts);
  const fOut = measureFreq(r.out, SR, SR, fTarget, 0.05);
  const cents = Math.abs(Tune.ratioToCents(fOut / fTarget));
  check(cents < 25, 'sawtooth 148 Hz', `sortie ${fOut.toFixed(2)} Hz (cible ${fTarget.toFixed(2)}, ecart ${cents.toFixed(1)} cents)`);
}

// --- 3. transparence ------------------------------------------------------
console.log('\nTEST 3 — transparence (amount = 0)');
{
  const x = saw(196, SR * 3);
  const r = Tune.process(x, SR, Object.assign({}, baseOpts, { amount: 0 }));
  let best = -2, bestLag = 0;
  for (let lag = 1200; lag <= 2200; lag++) {
    const c = corr(x, r.out, lag, 20000, 60000);
    if (c > best) { best = c; bestLag = lag; }
  }
  check(best > 0.95, 'signal reconstruit a l identique', `correlation ${best.toFixed(4)} (retard ${bestLag})`);
}

// --- 4. gammes ------------------------------------------------------------
console.log('\nTEST 4 — gammes');
{
  const counts = [12, 7, 7, 7, 5, 5, 6, 7];
  Tune.SCALE_MASKS.forEach((m, i) => {
    let c = 0;
    for (let b = 0; b < 12; b++) if ((m >> b) & 1) c++;
    check(c === counts[i], `gamme ${Tune.SCALE_NAMES[i]}`, `${c} notes`);
  });
  const majorMask = Tune.maskFor(0, 1, 0);            // C majeur
  check(Tune.nearestNote(66, majorMask) === 65 || Tune.nearestNote(66, majorMask) === 67,
        'C majeur : F#4 quantifie vers F ou G', `-> ${Tune.nearestNote(66, majorMask)}`);
}

// --- 5. robustesse --------------------------------------------------------
console.log('\nTEST 5 — robustesse');
{
  const n = SR * 2;
  const silent = new Float32Array(n);
  const r1 = Tune.process(silent, SR, baseOpts);
  let peak = 0;
  for (let i = 0; i < n; i++) peak = Math.max(peak, Math.abs(r1.out[i]));
  check(peak < 1e-6, 'silence -> silence', `crete ${peak.toExponential(1)}`);

  const noise = new Float32Array(n);
  let seed = 12345;
  for (let i = 0; i < n; i++) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    noise[i] = ((seed / 0x7fffffff) * 2 - 1) * 0.3;
  }
  const r2 = Tune.process(noise, SR, baseOpts);
  let finite = true, peak2 = 0;
  for (let i = 0; i < n; i++) {
    if (!isFinite(r2.out[i])) finite = false;
    peak2 = Math.max(peak2, Math.abs(r2.out[i]));
  }
  check(finite, 'bruit -> valeurs finies', `crete ${peak2.toFixed(3)}`);
  check(peak2 < 1.0, 'bruit -> pas d emballement', '');
}

// --- 6. retune ------------------------------------------------------------
console.log('\nTEST 6 — retune speed');
{
  const x = sine(227, SR * 3);
  const fast = Tune.process(x, SR, Object.assign({}, baseOpts, { retuneMs: 0 }));
  const slow = Tune.process(x, SR, Object.assign({}, baseOpts, { retuneMs: 400 }));
  const fFast = measureFreq(fast.out, SR * 0.2, 12000, 233, 0.05);
  const fSlow = measureFreq(slow.out, SR * 0.2, 12000, 233, 0.05);
  const diffCents = Math.abs(Tune.ratioToCents(fFast / fSlow));
  check(diffCents > 10, 'retune 0 ms vs 400 ms', `${fFast.toFixed(1)} Hz vs ${fSlow.toFixed(1)} Hz (${diffCents.toFixed(0)} cents)`);
}

// --- 7. performance -------------------------------------------------------
console.log('\nTEST 7 — performance');
{
  const x = saw(180, SR * 10);   // 10 secondes
  const t0 = Date.now();
  const r = Tune.process(x, SR, baseOpts);
  const dt = (Date.now() - t0) / 1000;
  check(true, 'traitement de 10 s d audio', `${dt.toFixed(1)} s (${(dt / 10).toFixed(2)}x temps reel)`);
  check(r.frames.length > 0, 'frames de detection', `${r.frames.length} frames`);
}

console.log(`\n=== ${failures === 0 ? 'TOUS LES TESTS PASSENT' : failures + ' ECHEC(S)'} ===`);
process.exit(failures === 0 ? 0 : 1);
