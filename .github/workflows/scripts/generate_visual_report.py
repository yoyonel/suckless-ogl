import os
import sys
import json
import re

def parse_ref_name(filename):
    # Pattern: ref_<view>_<mode>_<effect>.png or ref_<view>.png
    # Examples:
    # ref_front.png -> view=front, mode=default, effect=none
    # ref_front_subtle_bloom.png -> view=front, mode=subtle, effect=bloom
    # ref_front_default_auto_exposure.png -> view=front, mode=default, effect=auto_exposure

    basename = filename.replace("ref_", "").replace(".png", "")
    parts = basename.split("_")

    if len(parts) == 1:
        return parts[0], "default", "none"
    elif len(parts) == 3:
        return parts[0], parts[1], parts[2]
    elif len(parts) == 4: # auto_exposure has an underscore
        return parts[0], parts[1], "_".join(parts[2:])
    else:
        # Fallback
        return parts[0], "unknown", "_".join(parts[1:])

def generate_effect_diff(base_path, effect_path, out_path):
    """Generates a high-contrast difference map between baseline and effect image."""
    try:
        from PIL import Image, ImageChops, ImageEnhance
        base = Image.open(base_path).convert("RGB")
        eff = Image.open(effect_path).convert("RGB")
        # Absolute difference
        diff = ImageChops.difference(base, eff)
        # Enhance contrast to make changes visible
        enhancer = ImageEnhance.Contrast(diff)
        diff = enhancer.enhance(5.0) # 5x contrast
        diff.save(out_path)
        return True
    except ImportError:
        # Fallback to ImageMagick if Pillow is missing
        import subprocess
        try:
            # convert base.png effect.png -compose difference -composite -evaluate multiply 5 out.png
            subprocess.run([
                "convert", base_path, effect_path,
                "-compose", "difference", "-composite",
                "-evaluate", "multiply", "5",
                out_path
            ], check=True, capture_output=True)
            return True
        except Exception as e:
            print(f"[WARN] Failed to generate effect diff via ImageMagick: {e}")
            return False
    except Exception as e:
        print(f"[WARN] Failed to generate effect diff via Pillow: {e}")
        return False

def generate_report(sha, repository, pr_number=None):
    test_dir = "tests"
    html_index_path = "build-coverage/coverage_report/visual_tests/index.html"
    comment_path = ".github/workflows/scripts/comment.md"

    # Discovery
    ref_files = [f for f in os.listdir(test_dir) if f.startswith("ref_") and f.endswith(".png")]
    ref_files.sort()

    html_dir = os.path.dirname(html_index_path)
    os.makedirs(html_dir, exist_ok=True)

    import shutil

    data = []
    views = set()
    modes = set()
    effects = set()

    # First pass to find baselines (none)
    baselines = {} # (view, mode) -> path
    for f in ref_files:
        view, mode, effect = parse_ref_name(f)
        if effect == "none":
            baselines[(view, mode)] = os.path.join(test_dir, f)

    for f in ref_files:
        view, mode, effect = parse_ref_name(f)
        basename = f.replace("ref_", "").replace(".png", "")

        actual_name = f"failed_actual_{basename}.png"
        diff_name = f"failed_diff_{basename}.png"
        effect_diff_name = f"effect_diff_{basename}.png"

        entry = {
            "file": f,
            "view": view,
            "mode": mode,
            "effect": effect,
            "actual": actual_name,
            "diff": diff_name,
            "effect_diff": None,
            "status": "PASS",
            "baseline": None
        }

        # Copy reference
        shutil.copy2(os.path.join(test_dir, f), os.path.join(html_dir, f))

        # Handle failures
        if os.path.exists(os.path.join(test_dir, actual_name)):
            entry["status"] = "FAIL"
            shutil.copy2(os.path.join(test_dir, actual_name), os.path.join(html_dir, actual_name))
            shutil.copy2(os.path.join(test_dir, diff_name), os.path.join(html_dir, diff_name))
        else:
            entry["actual"] = f # Self-reference for hover/compare on success

        # Handle effect visualization
        if effect != "none" and (view, mode) in baselines:
            base_path = baselines[(view, mode)]
            effect_path = os.path.join(test_dir, f)
            out_diff_path = os.path.join(html_dir, effect_diff_name)
            if generate_effect_diff(base_path, effect_path, out_diff_path):
                entry["baseline"] = os.path.basename(base_path)
                entry["effect_diff"] = effect_diff_name

        data.append(entry)
        views.add(view)
        modes.add(mode)
        effects.add(effect)

    # Sort categories
    sorted_views = sorted(list(views))
    sorted_modes = sorted(list(modes))
    sorted_effects = sorted(list(effects))

    # HTML template with premium design and selective menu
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Rapport de Régression Visuelle</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {{
      --bg-color: #0d1117;
      --card-bg: #161b22;
      --border-color: #30363d;
      --text-main: #c9d1d9;
      --text-dim: #8b949e;
      --accent: #58a6ff;
      --pass: #238636;
      --fail: #f85149;
    }}
    body {{
      font-family: 'Inter', sans-serif;
      background-color: var(--bg-color);
      color: var(--text-main);
      margin: 0;
      padding: 40px 20px;
      line-height: 1.6;
    }}
    .container {{ max-width: 1200px; margin: 0 auto; }}
    h1 {{ text-align: center; font-weight: 700; margin-bottom: 8px; }}
    .subtitle {{ text-align: center; color: var(--text-dim); margin-bottom: 40px; font-size: 0.95rem; }}

    .controls {{
      background: var(--card-bg);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 24px;
      margin-bottom: 30px;
      display: flex;
      flex-wrap: wrap;
      gap: 24px;
      justify-content: center;
      box-shadow: 0 8px 24px rgba(0,0,0,0.2);
    }}
    .field {{ display: flex; flex-direction: column; gap: 8px; }}
    .field label {{ font-size: 0.75rem; font-weight: 600; text-transform: uppercase; color: var(--accent); letter-spacing: 0.05em; }}
    select {{
      background: #0d1117;
      color: var(--text-main);
      border: 1px solid var(--border-color);
      padding: 8px 16px;
      border-radius: 6px;
      min-width: 180px;
      font-size: 0.9rem;
      cursor: pointer;
    }}
    .option-fail {{ color: var(--fail); font-weight: 600; }}

    .mode-toggle {{
      display: flex;
      background: #0d1117;
      border: 1px solid var(--border-color);
      border-radius: 8px;
      padding: 4px;
      gap: 4px;
      align-self: flex-end;
    }}
    .mode-btn {{
      padding: 6px 16px;
      border-radius: 6px;
      border: none;
      background: transparent;
      color: var(--text-dim);
      font-size: 0.85rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
    }}
    .mode-btn.active {{
      background: var(--card-bg);
      color: var(--accent);
      box-shadow: 0 2px 8px rgba(0,0,0,0.3);
    }}

    .status-area {{ text-align: center; margin-bottom: 20px; min-height: 24px; }}
    .status-badge {{
      display: inline-block;
      padding: 4px 12px;
      border-radius: 20px;
      font-size: 0.75rem;
      font-weight: 700;
      letter-spacing: 0.02em;
    }}
    .status-pass {{ background: rgba(35, 134, 54, 0.15); color: #3fb950; border: 1px solid rgba(63, 185, 80, 0.3); }}
    .status-fail {{ background: rgba(248, 81, 73, 0.15); color: #f85149; border: 1px solid rgba(248, 81, 73, 0.3); }}

    .main-display {{
      display: grid;
      grid-template-columns: 1fr;
      gap: 30px;
      margin-top: 20px;
    }}

    /* Slider Container */
    .comparison-slider {{
      position: relative;
      width: 100%;
      aspect-ratio: 1024 / 768;
      border-radius: 12px;
      overflow: hidden;
      border: 1px solid var(--border-color);
      box-shadow: 0 12px 48px rgba(0,0,0,0.5);
      background: #000;
      margin: 0 auto;
      max-width: 1024px;
    }}
    .comparison-slider img {{
      position: absolute;
      top: 0; left: 0;
      width: 100%; height: 100%;
      object-fit: contain;
      user-select: none;
    }}
    .slider-before {{ z-index: 10; clip-path: inset(0 50% 0 0); }}
    .slider-after {{ z-index: 5; }}

    .slider-handle {{
      position: absolute;
      top: 0; bottom: 0;
      left: 50%;
      width: 4px;
      background: white;
      z-index: 20;
      cursor: ew-resize;
      box-shadow: 0 0 10px rgba(0,0,0,0.5);
    }}
    .handle-circle {{
      position: absolute;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      width: 40px; height: 40px;
      background: white;
      border-radius: 50%;
      display: flex;
      align-items: center; justify-content: center;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
    }}
    .handle-circle::before, .handle-circle::after {{
      content: '';
      border: solid #000;
      border-width: 0 3px 3px 0;
      display: inline-block;
      padding: 3px;
    }}
    .handle-circle::before {{ transform: rotate(135deg); margin-right: 4px; }}
    .handle-circle::after {{ transform: rotate(-45deg); margin-left: 4px; }}

    .label-tag {{
        position: absolute;
        bottom: 20px;
        background: rgba(0,0,0,0.7);
        padding: 4px 12px;
        border-radius: 6px;
        font-size: 0.75rem;
        font-weight: 700;
        z-index: 25;
        border: 1px solid rgba(255,255,255,0.1);
        backdrop-filter: blur(4px);
    }}
    .label-left {{ left: 20px; color: var(--accent); }}
    .label-right {{ right: 20px; color: #a371f7; }}

    /* Zoom Lens */
    .zoom-lens {{
      position: absolute;
      border: 2px solid white;
      border-radius: 50%;
      cursor: none;
      width: 150px;
      height: 150px;
      z-index: 100;
      display: none;
      pointer-events: none;
      box-shadow: 0 0 20px rgba(0,0,0,0.5), inset 0 0 10px rgba(0,0,0,0.5);
      background-repeat: no-repeat;
      overflow: hidden;
    }}
    .zoom-lens::after {{
      content: '';
      position: absolute;
      top: 50%; left: 0; right: 0;
      height: 2px; background: white;
      transform: translateY(-50%);
      display: none; /* Only show in split mode? */
    }}

    .diff-map-section {{
      text-align: center;
    }}
    .diff-map-section h3 {{ font-size: 0.85rem; color: var(--text-dim); text-transform: uppercase; margin-bottom: 12px; }}
    .diff-map-container img {{
      max-width: 100%;
      border-radius: 8px;
      border: 1px solid var(--border-color);
    }}

    .no-variant {{
      text-align: center; padding: 100px; color: var(--text-dim); font-style: italic; border: 2px dashed var(--border-color); border-radius: 12px;
    }}

    .footer {{ margin-top: 60px; color: var(--text-dim); border-top: 1px solid var(--border-color); padding-top: 20px; text-align: center; }}
    .footer a {{ color: var(--accent); text-decoration: none; font-weight: 600; }}
    .footer a:hover {{ text-decoration: underline; }}

    /* Language Switcher */
    .lang-switcher {{
      position: fixed;
      top: 16px;
      right: 20px;
      display: flex;
      gap: 6px;
      z-index: 1000;
    }}
    .lang-btn {{
      padding: 5px 12px;
      border-radius: 6px;
      border: 1px solid var(--border-color);
      background: var(--card-bg);
      color: var(--text-dim);
      font-size: 0.8rem;
      font-weight: 700;
      cursor: pointer;
      letter-spacing: 0.05em;
      transition: all 0.2s;
    }}
    .lang-btn.active {{
      background: var(--accent);
      color: #0d1117;
      border-color: var(--accent);
    }}
    .lang-btn:hover:not(.active) {{
      border-color: var(--accent);
      color: var(--accent);
    }}
  </style>
</head>
<body>
  <div class="lang-switcher">
    <button class="lang-btn" id="langEN" onclick="setLang('en')">EN</button>
    <button class="lang-btn" id="langFR" onclick="setLang('fr')">FR</button>
  </div>
  <div class="container">
    <h1 id="i18n-title"></h1>
    <div class="subtitle"><span id="i18n-subtitle-intro"></span>
      <span style="color:var(--accent)" id="labelDescLeft"></span>
      <span id="i18n-subtitle-vs"> vs </span>
      <span style="color:#a371f7" id="labelDescRight"></span></div>

    <div class="controls">
      <div class="field">
        <label id="i18n-label-view"></label>
        <select id="viewSelect"></select>
      </div>
      <div class="field">
        <label id="i18n-label-mode"></label>
        <select id="modeSelect"></select>
      </div>
      <div class="field">
        <label id="i18n-label-effect"></label>
        <select id="effectSelect"></select>
      </div>

      <div class="mode-toggle">
        <button id="btnPR" class="mode-btn active"></button>
        <button id="btnEffect" class="mode-btn"></button>
      </div>
    </div>

    <div id="statusArea" class="status-area"></div>

    <div style="text-align: center; margin-bottom: 20px;">
        <button id="btnZoom" class="mode-btn" style="border: 1px solid var(--border-color); display: inline-flex; align-items: center; gap: 8px;">
            <span id="zoomIcon">🔍</span> <span id="i18n-zoom-label"></span>
        </button>
    </div>

    <div id="mainDisplay" class="main-display">
      <div id="compareWrapper" class="comparison-slider zoomable">
        <div id="zoomLens" class="zoom-lens"></div>
        <img id="refImg" class="slider-before" src="" alt="">
        <img id="actualImg" class="slider-after" src="" alt="">
        <div id="sliderHandle" class="slider-handle">
          <div class="handle-circle"></div>
        </div>
        <div id="labelLeft" class="label-tag label-left"></div>
        <div id="labelRight" class="label-tag label-right"></div>
      </div>

      <div id="diffSection" class="diff-map-section" style="display:none">
        <h3 id="diffTitle"></h3>
        <div class="diff-map-container zoomable" style="position: relative; display: inline-block; overflow: hidden; border-radius: 8px;">
          <img id="diffImg" src="" alt="">
        </div>
      </div>
    </div>

    <div id="noVariantMessage" class="no-variant" style="display:none">
      <span id="i18n-no-variant"></span>
    </div>

    <div class="footer">
        <a id="i18n-back-link" href='../index.html'></a>
    </div>
  </div>

  <script>
    const data = {json.dumps(data)};
    const sortedViews = {json.dumps(sorted_views)};

    // ── i18n ────────────────────────────────────────────────────────────────
    const I18N = {{
      en: {{
        title:          'Visual Regression Analysis',
        subtitleIntro:  'Use the slider to compare:',
        subtitleVs:     'vs',
        labelView:      'View',
        labelMode:      'Mode',
        labelEffect:    'Effect',
        btnPR:          'PR Comparison',
        btnEffect:      'Effect Preview',
        zoomOn:         'Enable Magnifier',
        zoomOff:        'Disable Magnifier',
        labelRef:       'REFERENCE',
        labelRender:    'PR RENDER',
        descRef:        'Baseline',
        descRender:     'Render',
        labelNoEffect:  'NO EFFECT (NONE)',
        labelWithEffect: 'WITH EFFECT',
        descNoEffect:   'No Effect',
        descWithEffect: 'With Effect',
        statusPass:     '\u25cf Visual Match',
        statusFail:     '\u25cf Regression Detected',
        statusEffect:   'Effect Preview',
        diffTitlePR:    'DIFFERENCE MAP (REF vs PR)',
        diffTitleInit:  'DIFFERENCE MAP (x5 CONTRAST)',
        diffTitleEffect: 'EFFECT INTENSITY (DIFF MAP x5)',
        noVariant:      'No reference image for this combination.',
        backLink:       '\u2190 Back to Coverage Report',
      }},
      fr: {{
        title:          'Analyse de R\u00e9gression Visuelle',
        subtitle:       'Utilisez le slider pour comparer : <span style="color:var(--accent)" id="labelDescLeft">R\u00e9f\u00e9rence</span> vs <span style="color:#a371f7" id="labelDescRight">Rendu</span>',
        labelView:      'Vue',
        labelMode:      'Mode',
        labelEffect:    'Effet',
        btnPR:          'Comparaison PR',
        btnEffect:      "Visualisation d'Effet",
        zoomOn:         'Activer la Loupe',
        zoomOff:        'D\u00e9sactiver la Loupe',
        labelRef:       'R\u00c9F\u00c9RENCE',
        labelRender:    'RENDU PR',
        descRef:        'R\u00e9f\u00e9rence',
        descRender:     'Rendu',
        labelNoEffect:  'SANS EFFET (NONE)',
        labelWithEffect: 'AVEC EFFET',
        descNoEffect:   'Sans Effet',
        descWithEffect: 'Avec Effet',
        statusPass:     '\u25cf Correspondance Visuelle',
        statusFail:     '\u25cf R\u00e9gression D\u00e9tect\u00e9e',
        statusEffect:   "Visualisation de l'effet",
        diffTitlePR:    'CARTE DES DIFF\u00c9RENCES (R\u00c9F vs PR)',
        diffTitleInit:  'CARTE DES DIFF\u00c9RENCES (x5 CONTRASTE)',
        diffTitleEffect: "INTENSIT\u00c9 DE L'EFFET (CARTE DIFF x5)",
        noVariant:      'Aucune image de r\u00e9f\u00e9rence pour cette combinaison.',
        backLink:       '\u2190 Retour au Rapport de Couverture',
      }},
    }};

    let currentLang = localStorage.getItem('visual-report-lang') || 'en';

    function applyLang() {{
      const t = I18N[currentLang];
      document.documentElement.lang = currentLang;
      document.title = t.title;
      document.getElementById('i18n-title').textContent = t.title;
      document.getElementById('i18n-subtitle-intro').textContent = t.subtitleIntro;
      document.getElementById('i18n-subtitle-vs').textContent = t.subtitleVs;
      document.getElementById('i18n-label-view').textContent = t.labelView;
      document.getElementById('i18n-label-mode').textContent = t.labelMode;
      document.getElementById('i18n-label-effect').textContent = t.labelEffect;
      document.getElementById('btnPR').textContent = t.btnPR;
      document.getElementById('btnEffect').textContent = t.btnEffect;
      document.getElementById('i18n-zoom-label').textContent = isZoomActive ? t.zoomOff : t.zoomOn;
      document.getElementById('i18n-no-variant').textContent = t.noVariant;
      document.getElementById('i18n-back-link').textContent = t.backLink;
      document.getElementById('langEN').classList.toggle('active', currentLang === 'en');
      document.getElementById('langFR').classList.toggle('active', currentLang === 'fr');
    }}

    function setLang(lang) {{
      currentLang = lang;
      localStorage.setItem('visual-report-lang', lang);
      applyLang();
      updateDisplay();
    }}
    // ────────────────────────────────────────────────────────────────────────

    const vSel = document.getElementById('viewSelect');
    const mSel = document.getElementById('modeSelect');
    const eSel = document.getElementById('effectSelect');
    const btnPR = document.getElementById('btnPR');
    const btnEffect = document.getElementById('btnEffect');

    let currentComparisonMode = 'PR'; // 'PR' or 'EFFECT'

    function populateViews() {{
        vSel.innerHTML = '';
        sortedViews.forEach(v => {{
            const opt = document.createElement('option');
            opt.value = v;
            opt.textContent = v;
            if (data.some(d => d.view === v && d.status === 'FAIL')) opt.className = 'option-fail';
            vSel.appendChild(opt);
        }});
    }}

    function updateModes() {{
        const view = vSel.value;
        const currentMode = mSel.value;
        const availableModes = [...new Set(data.filter(d => d.view === view).map(d => d.mode))].sort();

        mSel.innerHTML = '';
        availableModes.forEach(m => {{
            const opt = document.createElement('option');
            opt.value = m;
            opt.textContent = m;
            if (data.some(d => d.view === view && d.mode === m && d.status === 'FAIL')) opt.className = 'option-fail';
            mSel.appendChild(opt);
        }});

        if (availableModes.includes(currentMode)) mSel.value = currentMode; else mSel.selectedIndex = 0;
        updateEffects();
    }}

    function updateEffects() {{
        const view = vSel.value;
        const mode = mSel.value;
        const currentEffect = eSel.value;
        const availableEffects = [...new Set(data.filter(d => d.view === view && d.mode === mode).map(d => d.effect))].sort();

        eSel.innerHTML = '';
        availableEffects.forEach(e => {{
            const opt = document.createElement('option');
            opt.value = e;
            opt.textContent = e;
            if (data.some(d => d.view === view && d.mode === mode && d.effect === e && d.status === 'FAIL')) opt.className = 'option-fail';
            eSel.appendChild(opt);
        }});

        if (availableEffects.includes(currentEffect)) eSel.value = currentEffect; else eSel.selectedIndex = 0;

        // Hide effect btn if only 'none' is available
        btnEffect.style.display = (availableEffects.length > 1) ? 'block' : 'none';
        if (availableEffects.length <= 1 && currentComparisonMode === 'EFFECT') {{
            setMode('PR');
        }}

        updateDisplay();
    }}

    function updateDisplay() {{
        const view = vSel.value;
        const mode = mSel.value;
        const effect = eSel.value;

        const match = data.find(d => d.view === view && d.mode === mode && d.effect === effect);

        const wrapper = document.getElementById('mainDisplay');
        const refImg = document.getElementById('refImg');
        const actualImg = document.getElementById('actualImg');
        const diffSection = document.getElementById('diffSection');
        const diffImg = document.getElementById('diffImg');
        const statusArea = document.getElementById('statusArea');
        const emptyMsg = document.getElementById('noVariantMessage');
        const labL = document.getElementById('labelLeft');
        const labR = document.getElementById('labelRight');
        const dTitle = document.getElementById('diffTitle');
        const descL = document.getElementById('labelDescLeft');
        const descR = document.getElementById('labelDescRight');

        if (match) {{
            wrapper.style.display = 'grid';
            emptyMsg.style.display = 'none';

            const t = I18N[currentLang];
            if (currentComparisonMode === 'PR') {{
                refImg.src = match.file;
                actualImg.src = match.actual;
                labL.textContent = t.labelRef;
                labR.textContent = t.labelRender;
                labL.style.color = 'var(--accent)';
                descL.textContent = t.descRef;
                descR.textContent = t.descRender;

                if (match.status === 'FAIL') {{
                    statusArea.innerHTML = '<span class="status-badge status-fail">' + t.statusFail + '</span>';
                    diffSection.style.display = 'block';
                    diffImg.src = match.diff;
                    dTitle.textContent = t.diffTitlePR;
                }} else {{
                    statusArea.innerHTML = '<span class="status-badge status-pass">' + t.statusPass + '</span>';
                    diffSection.style.display = 'none';
                }}
            }} else {{
                // Effect Visualization Mode
                refImg.src = match.baseline || match.file;
                actualImg.src = match.file;
                labL.textContent = t.labelNoEffect;
                labR.textContent = t.labelWithEffect + ' (' + effect.toUpperCase() + ')';
                labL.style.color = 'var(--text-dim)';
                descL.textContent = t.descNoEffect;
                descR.textContent = t.descWithEffect;
                statusArea.innerHTML = '<span class="status-badge status-pass">' + t.statusEffect + '</span>';

                if (match.effect_diff) {{
                    diffSection.style.display = 'block';
                    diffImg.src = match.effect_diff;
                    dTitle.textContent = t.diffTitleEffect;
                }} else {{
                    diffSection.style.display = 'none';
                }}
            }}
        }} else {{
            wrapper.style.display = 'none';
            statusArea.innerHTML = '';
            emptyMsg.style.display = 'block';
        }}
    }}

    function setMode(mode) {{
        currentComparisonMode = mode;
        btnPR.classList.toggle('active', mode === 'PR');
        btnEffect.classList.toggle('active', mode === 'EFFECT');
        updateDisplay();
    }}

    btnPR.onclick = () => setMode('PR');
    btnEffect.onclick = () => setMode('EFFECT');

    // Zoom & Slider Logic
    const slider = document.getElementById('compareWrapper');
    const handle = document.getElementById('sliderHandle');
    const beforeImg = document.querySelector('.slider-before');
    const lens = document.getElementById('zoomLens');
    const btnZoom = document.getElementById('btnZoom');
    const zoomIcon = document.getElementById('zoomIcon');

    let isResizing = false;
    let isZoomActive = false;
    const zoomLevel = 2.5;

    function setSliderPos(x) {{
        const rect = slider.getBoundingClientRect();
        let pos = ((x - rect.left) / rect.width) * 100;
        pos = Math.max(0, Math.min(100, pos));
        handle.style.left = pos + '%';
        beforeImg.style.clipPath = `inset(0 ${{100 - pos}}% 0 0)`;
    }}

    slider.addEventListener('mousedown', (e) => {{
        if (!isZoomActive) {{
            isResizing = true;
            setSliderPos(e.pageX);
        }}
    }});
    window.addEventListener('mouseup', () => isResizing = false);

    window.addEventListener('mousemove', (e) => {{
        if (isResizing) setSliderPos(e.pageX);
        if (isZoomActive) moveLens(e);
    }});

    // Zoom Logic
    btnZoom.onclick = () => {{
        isZoomActive = !isZoomActive;
        btnZoom.classList.toggle('active', isZoomActive);
        zoomIcon.textContent = isZoomActive ? '👁️\u200d🗨️' : '🔍';
        const t = I18N[currentLang];
        document.getElementById('i18n-zoom-label').textContent = isZoomActive ? t.zoomOff : t.zoomOn;
        lens.style.display = isZoomActive ? 'block' : 'none';

        // Update cursors for all zoomable areas
        document.querySelectorAll('.zoomable').forEach(el => {{
            el.style.cursor = isZoomActive ? 'none' : 'crosshair';
        }});
    }};

    function moveLens(e) {{
        // Find which zoomable container we are over
        const container = e.target.closest('.zoomable');
        if (!container || !isZoomActive) {{
            lens.style.display = 'none';
            return;
        }}

        const rect = container.getBoundingClientRect();
        const x = e.pageX - rect.left - window.pageXOffset;
        const y = e.pageY - rect.top - window.pageYOffset;

        if (x < 0 || x > rect.width || y < 0 || y > rect.height) {{
            lens.style.display = 'none';
            return;
        }}

        // Move lens to be child of current container if not already
        if (lens.parentElement !== container) {{
            container.appendChild(lens);
        }}

        lens.style.display = 'block';
        lens.style.left = (x - lens.offsetWidth / 2) + 'px';
        lens.style.top = (y - lens.offsetHeight / 2) + 'px';

        let targetImg;
        if (container.classList.contains('comparison-slider')) {{
            const sliderPos = parseFloat(handle.style.left) || 50;
            const xPercent = (x / rect.width) * 100;
            targetImg = (xPercent < sliderPos) ? document.getElementById('refImg') : document.getElementById('actualImg');
        }} else {{
            targetImg = container.querySelector('img');
        }}

        lens.style.backgroundImage = `url("${{targetImg.src}}")`;
        lens.style.backgroundSize = (rect.width * zoomLevel) + "px " + (rect.height * zoomLevel) + "px";
        lens.style.backgroundPosition = "-" + (x * zoomLevel - lens.offsetWidth / 2) + "px -" + (y * zoomLevel - lens.offsetHeight / 2) + "px";
    }}

    document.querySelectorAll('.zoomable').forEach(el => {{
        el.addEventListener('mouseenter', () => {{ if (isZoomActive) lens.style.display = 'block'; }});
        el.addEventListener('mouseleave', () => {{ lens.style.display = 'none'; }});
    }});

    // Initial calls
    vSel.onchange = updateModes;
    mSel.onchange = updateModes;
    eSel.onchange = updateDisplay;

    applyLang();
    populateViews();
    updateModes();
  </script>
</body>
</html>"""

    with open(html_index_path, "w") as f:
        f.write(html)

    # Markdown Comment Generation
    comment = f"## 🎨 Visual Regression Report ({sha})\n\n"

    failing = [d for d in data if d["status"] == "FAIL"]

    if not failing:
        comment += "✅ **All visual tests passed!** (Analyzed versions: "
        comment += ", ".join(sorted_effects) + ")\n"
    else:
        comment += f"⚠️ **{len(failing)} regressions detected!**\n\n"
        comment += "| Variant | Reference | Comparison | Diff |\n"
        comment += "|:---:|:---:|:---:|:---:|\n"

        for entry in failing:
            variant = f"{entry['view']} / {entry['mode']} / {entry['effect']}"
            ref_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/{entry['file']}?raw=true"
            actual_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/{entry['actual']}?raw=true"
            diff_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/{entry['diff']}?raw=true"

            ref_img = f"![Ref]({ref_url})"
            actual_img = f"![Actual]({actual_url})"
            diff_img = f"![Diff]({diff_url})"

            comment += f"| **{variant}** | {ref_img} | {actual_img} | {diff_img} |\n"

    comment += "\n> [!TIP]\n"
    if pr_number:
        interactive_url = f"https://suckless-ogl-pr-{pr_number}.surge.sh/coverage/visual_tests/index.html"
        comment += f"> Accédez au **[Rapport Interactif Complet]({interactive_url})** pour filtrer par vue/mode/effet et utiliser la loupe.\n"
    else:
        comment += "> Accédez au rapport interactif complet dans l'onglet **Summary** du CI pour filtrer par vue/mode/effet.\n"

    with open(comment_path, "w") as f:
        f.write(comment)

    # Set GITHUB_ENV if available
    github_env = os.getenv('GITHUB_ENV')
    if github_env:
        with open(github_env, "a") as env_file:
            env_file.write("VISUAL_COMMENT<<EOF\n")
            env_file.write(comment)
            env_file.write("\nEOF\n")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: generate_visual_report.py <sha> <repository> [pr_number]")
        sys.exit(1)

    pr_num = sys.argv[3] if len(sys.argv) > 3 else None
    generate_report(sys.argv[1], sys.argv[2], pr_num)
