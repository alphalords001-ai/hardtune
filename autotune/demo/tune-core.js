/* ============================================================================
 *  tune-core.js — moteur de correction de hauteur, portage JavaScript
 *  du moteur C++ (PitchDetector + NoteMapper + TuneCore).
 *
 *  Sert à la démo navigateur : elle tourne hors ligne, sans installation.
 *  Aucune dépendance, aucun réseau.
 * ========================================================================== */
(function (global) {
  'use strict';

  var NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  var SCALE_NAMES = ['Chromatic', 'Major', 'Minor', 'Harmonic Minor',
                     'Pentatonic Maj', 'Pentatonic Min', 'Blues', 'Dorian'];
  // masques 12 bits, identiques au C++ (bit 0 = C)
  var SCALE_MASKS = [0xFFF, 0xAB5, 0x5AD, 0x9AD, 0x295, 0x4A9, 0x4E9, 0x6AD];

  function centsToRatio(c) { return Math.pow(2, c / 1200); }
  function ratioToCents(r) { return 1200 * Math.log2(r); }
  function hzToMidi(h)     { return 69 + 12 * Math.log2(h / 440); }
  function midiToHz(m)     { return 440 * Math.pow(2, (m - 69) / 12); }

  function maskFor(key, scaleIndex, customMask) {
    var base = (scaleIndex === 8) ? (customMask | 0) : SCALE_MASKS[scaleIndex];
    // on décale pour que le masque soit exprimé relativement à la tonalité
    var abs = 0;
    for (var i = 0; i < 12; i++) {
      if ((base >> i) & 1) abs |= (1 << ((i + key) % 12));
    }
    return abs;
  }

  function isAllowed(pc, mask) {
    var p = ((pc % 12) + 12) % 12;
    return ((mask >> p) & 1) !== 0;
  }

  function nearestNote(midiF, mask) {
    var base = Math.round(midiF);
    for (var d = 0; d <= 6; d++) {
      if (isAllowed(base - d, mask)) return base - d;
      if (isAllowed(base + d, mask)) return base + d;
    }
    return base;
  }

  /* ------------------------------------------------------------------------
   *  Détection de hauteur (YIN)
   * ---------------------------------------------------------------------- */
  function detect(x, sr, hop, win) {
    hop = hop || 512;
    win = win || 1024;

    var tauMin = Math.max(2, Math.floor(sr / 1200));
    var tauMax = Math.min(win >> 1, Math.ceil(sr / 80));
    var integ  = win - tauMax;
    if (integ < 32) return [];

    var d  = new Float64Array(tauMax + 2);
    var cm = new Float64Array(tauMax + 2);
    var frames = [];

    for (var start = 0; start + win <= x.length; start += hop) {
      var tau, j, s, a, b, diff;

      for (tau = 0; tau <= tauMax; tau++) {
        s = 0;
        for (j = 0; j < integ; j++) {
          a = x[start + j];
          b = x[start + j + tau];
          diff = a - b;
          s += diff * diff;
        }
        d[tau] = s;
      }

      cm[0] = 1;
      var run = 0;
      for (tau = 1; tau <= tauMax; tau++) {
        run += d[tau];
        cm[tau] = run > 1e-20 ? (d[tau] * tau / run) : 1;
      }

      var best = -1;
      for (tau = tauMin; tau <= tauMax; tau++) {
        if (cm[tau] < 0.15) {
          while (tau + 1 <= tauMax && cm[tau + 1] < cm[tau]) tau++;
          best = tau;
          break;
        }
      }

      if (best < 0) {
        var bv = 1e9;
        for (tau = tauMin; tau <= tauMax; tau++) {
          if (cm[tau] < bv) { bv = cm[tau]; best = tau; }
        }
        if (best < 0 || bv > 0.45) {
          frames.push({ pos: start + win, hz: 0, conf: 0, voiced: false, ratio: 1 });
          continue;
        }
      }

      var tauRef = best;
      if (best > tauMin && best < tauMax) {
        var aa = cm[best - 1], bb = cm[best], cc = cm[best + 1];
        var den = aa - 2 * bb + cc;
        if (Math.abs(den) > 1e-20) {
          var sh = 0.5 * (aa - cc) / den;
          if (Math.abs(sh) < 1) tauRef += sh;
        }
      }

      frames.push({
        pos: start + win,
        hz: sr / tauRef,
        conf: Math.max(0, Math.min(1, 1 - cm[best])),
        voiced: true,
        ratio: 1
      });
    }
    return frames;
  }

  /* ------------------------------------------------------------------------
   *  Quantification sur la gamme
   * ---------------------------------------------------------------------- */
  function mapRatio(frames, opts) {
    var mask      = maskFor(opts.key | 0, opts.scale | 0, opts.customMask | 0);
    var amount    = opts.amount === undefined ? 1 : opts.amount;
    var transpose = opts.transpose | 0;
    var hop       = opts.hop || 512;
    var sr        = opts.sr || 44100;

    var alpha = 0;
    if (opts.retuneMs > 0.5) {
      var tauS = opts.retuneMs * 0.001;
      alpha = Math.min(0.9999, Math.max(0, Math.exp(-hop / (tauS * sr))));
    }

    var sticky = null, smoothed = 1;

    for (var i = 0; i < frames.length; i++) {
      var f = frames[i];
      var voiced = f.hz > 0 && f.conf > 0.30;
      f.voiced = voiced;

      if (!voiced) {
        smoothed += (1 - smoothed) * 0.25;
        f.ratio = smoothed;
        f.target = null;
        continue;
      }

      var mf = hzToMidi(f.hz);
      var cand = nearestNote(mf, mask);

      if (sticky === null) {
        sticky = cand;
      } else if (cand !== sticky) {
        var dC = Math.abs(mf - cand);
        var dS = Math.abs(mf - sticky);
        if (dS > 0.80 || dC + 0.08 < dS) sticky = cand;
      }

      var targetMidi = sticky + transpose;
      var cents = (targetMidi - mf) * 100 * amount;
      var target = Math.min(1.5, Math.max(0.75, centsToRatio(cents)));

      smoothed = alpha * smoothed + (1 - alpha) * target;
      f.ratio = smoothed;
      f.target = targetMidi;
    }
    return frames;
  }

  /* ------------------------------------------------------------------------
   *  Synthèse PSOLA
   * ---------------------------------------------------------------------- */
  function ratAt(frames, pos, fallback) {
    // dernière frame dont la position <= pos (recherche dichotomique)
    var lo = 0, hi = frames.length;
    while (lo < hi) {
      var mid = (lo + hi) >> 1;
      if (frames[mid].pos <= pos) lo = mid + 1; else hi = mid;
    }
    return lo === 0 ? frames[0] : frames[lo - 1];
  }

  function synth(x, sr, frames, formant, onProgress) {
    var n = x.length;
    var wet  = new Float32Array(n);
    var wsum = new Float32Array(n);

    var Tmin = Math.max(4, sr / 1200);
    var Tmax = sr / 80;
    var F = Math.min(2, Math.max(0.5, centsToRatio((formant || 0) * 4)));  // ±1 -> ±400 cents

    function read(pos) {
      if (pos <= 0 || pos >= n - 1) return 0;
      var i0 = pos | 0;
      var fr = pos - i0;
      return x[i0] + (x[i0 + 1] - x[i0]) * fr;
    }

    function bestMark(center, T) {
      var Ti = Math.min(512, T | 0);
      if (Ti < 16) return center;
      var lo = center - 0.5 * T, hi = center + 0.5 * T;
      var best = center, bestScore = -1e30;
      for (var q = lo; q <= hi; q += 2) {
        var sc = 0;
        for (var j = 0; j < Ti; j++) sc += read(q + j) * read(q + j - T);
        if (sc > bestScore) { bestScore = sc; best = q; }
      }
      return best;
    }

    var center = 0, lastMark = 0, hasMark = false,
        refinePeriod = 0, prevVoiced = false,
        lastGoodT = Tmax;

    var guard = 0;
    while (center < n && guard++ < n) {
      var f = ratAt(frames, Math.round(center), null);
      var voiced = f.voiced && f.hz > 0;
      var T = voiced ? Math.min(Tmax, Math.max(Tmin, sr / f.hz)) : Math.min(Tmax, Math.max(Tmin, lastGoodT));
      if (voiced) lastGoodT = T;

      var ratio = f.ratio;
      if (!(ratio > 0.01)) ratio = 1;
      ratio = Math.min(1.5, Math.max(0.75, ratio));

      var H = T / ratio;
      var L = 2 * T;

      // marque de pitch
      var onset   = voiced && !prevVoiced;
      var needRef = (!hasMark) || onset || (refinePeriod > 0 && Math.abs(T - refinePeriod) > 0.05 * refinePeriod);
      var mark;
      if (needRef) {
        mark = bestMark(center, T);
        refinePeriod = T;
      } else {
        var k = Math.round((center - lastMark) / T);
        if (k < 0) k = 0;
        mark = lastMark + k * T;
      }
      while (mark - center >  0.5 * T) mark -= T;
      while (mark - center < -0.5 * T) mark += T;

      lastMark = mark;
      hasMark = voiced;
      prevVoiced = voiced;

      // écriture du grain
      var c = Math.round(center);
      var half = Math.round(L / 2);
      for (var i = -half; i < half; i++) {
        var ph = (i + half + 0.5) / (half * 2);
        var w = 0.5 - 0.5 * Math.cos(2 * Math.PI * ph);
        if (w <= 1e-4) continue;
        var s = read(mark + i * F);
        var p = c + i;
        if (p < 0 || p >= n) continue;
        wet[p]  += s * w;
        wsum[p] += w;
      }

      center += H;
      if (onProgress && (guard & 1023) === 0) onProgress(center / n);
    }
    if (onProgress) onProgress(1);

    // normalisation + pondération voisé / dry
    var out = new Float32Array(n);
    var wv = 0;
    for (var i2 = 0; i2 < n; i2++) {
      var dry = x[i2];
      var wetS = (wsum[i2] > 0.15) ? wet[i2] / wsum[i2] : dry;

      var fr2 = ratAt(frames, i2, null);
      var tw = fr2.voiced ? 1 : 0;
      wv += (tw - wv) * 0.0057;   // ~4 ms à 44,1 kHz

      out[i2] = dry + (wetS - dry) * wv;
    }
    return out;
  }

  /* ------------------------------------------------------------------------
   *  Point d'entrée
   * ---------------------------------------------------------------------- */
  function process(x, sr, opts, onProgress) {
    var hop = 512, win = 1024;
    if (onProgress) onProgress(0);

    var frames = detect(x, sr, hop, win);
    mapRatio(frames, {
      key: opts.key, scale: opts.scale, customMask: opts.customMask,
      amount: opts.amount, retuneMs: opts.retuneMs,
      transpose: opts.transpose, hop: hop, sr: sr
    });

    var out = synth(x, sr, frames, opts.formant, onProgress);
    return { out: out, frames: frames };
  }

  var api = {
    NOTE_NAMES: NOTE_NAMES,
    SCALE_NAMES: SCALE_NAMES,
    SCALE_MASKS: SCALE_MASKS,
    process: process,
    detect: detect,
    mapRatio: mapRatio,
    synth: synth,
    hzToMidi: hzToMidi,
    midiToHz: midiToHz,
    ratioToCents: ratioToCents,
    centsToRatio: centsToRatio,
    nearestNote: nearestNote,
    maskFor: maskFor
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  global.Tune = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
