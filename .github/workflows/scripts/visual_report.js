    const data = __REPORT_DATA__;
    const sortedViews = __SORTED_VIEWS__;

    // ── i18n ────────────────────────────────────────────────────────────────
    const I18N = {
      en: {
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
      },
      fr: {
        title:          'Analyse de R\u00e9gression Visuelle',
        subtitleIntro:  'Utilisez le slider pour comparer :',
        subtitleVs:     'vs',
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
      },
    };

    let currentLang = localStorage.getItem('visual-report-lang') || 'en';
    let isZoomActive = false;

    function applyLang() {
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
    }

    function setLang(lang) {
      currentLang = lang;
      localStorage.setItem('visual-report-lang', lang);
      applyLang();
      updateDisplay();
    }
    // ────────────────────────────────────────────────────────────────────────

    const vSel = document.getElementById('viewSelect');
    const mSel = document.getElementById('modeSelect');
    const eSel = document.getElementById('effectSelect');
    const btnPR = document.getElementById('btnPR');
    const btnEffect = document.getElementById('btnEffect');

    let currentComparisonMode = 'PR'; // 'PR' or 'EFFECT'

    function populateViews() {
        vSel.innerHTML = '';
        sortedViews.forEach(v => {
            const opt = document.createElement('option');
            opt.value = v;
            opt.textContent = v;
            if (data.some(d => d.view === v && d.status === 'FAIL')) opt.className = 'option-fail';
            vSel.appendChild(opt);
        });
    }

    function updateModes() {
        const view = vSel.value;
        const currentMode = mSel.value;
        const availableModes = [...new Set(data.filter(d => d.view === view).map(d => d.mode))].sort();

        mSel.innerHTML = '';
        availableModes.forEach(m => {
            const opt = document.createElement('option');
            opt.value = m;
            opt.textContent = m;
            if (data.some(d => d.view === view && d.mode === m && d.status === 'FAIL')) opt.className = 'option-fail';
            mSel.appendChild(opt);
        });

        if (availableModes.includes(currentMode)) mSel.value = currentMode; else mSel.selectedIndex = 0;
        updateEffects();
    }

    function updateEffects() {
        const view = vSel.value;
        const mode = mSel.value;
        const currentEffect = eSel.value;
        const availableEffects = [...new Set(data.filter(d => d.view === view && d.mode === mode).map(d => d.effect))].sort();

        eSel.innerHTML = '';
        availableEffects.forEach(e => {
            const opt = document.createElement('option');
            opt.value = e;
            opt.textContent = e;
            if (data.some(d => d.view === view && d.mode === mode && d.effect === e && d.status === 'FAIL')) opt.className = 'option-fail';
            eSel.appendChild(opt);
        });

        if (availableEffects.includes(currentEffect)) eSel.value = currentEffect; else eSel.selectedIndex = 0;

        // Hide effect btn if only 'none' is available
        btnEffect.style.display = (availableEffects.length > 1) ? 'block' : 'none';
        if (availableEffects.length <= 1 && currentComparisonMode === 'EFFECT') {
            setMode('PR');
        }

        updateDisplay();
    }

    function updateDisplay() {
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

        if (match) {
            wrapper.style.display = 'grid';
            emptyMsg.style.display = 'none';

            const t = I18N[currentLang];
            if (currentComparisonMode === 'PR') {
                refImg.src = match.file;
                actualImg.src = match.actual;
                labL.textContent = t.labelRef;
                labR.textContent = t.labelRender;
                labL.style.color = 'var(--accent)';
                descL.textContent = t.descRef;
                descR.textContent = t.descRender;

                if (match.status === 'FAIL') {
                    statusArea.innerHTML = '<span class="status-badge status-fail">' + t.statusFail + '</span>';
                    diffSection.style.display = 'block';
                    diffImg.src = match.diff;
                    dTitle.textContent = t.diffTitlePR;
                } else {
                    statusArea.innerHTML = '<span class="status-badge status-pass">' + t.statusPass + '</span>';
                    diffSection.style.display = 'none';
                }
            } else {
                // Effect Visualization Mode
                refImg.src = match.baseline || match.file;
                actualImg.src = match.file;
                labL.textContent = t.labelNoEffect;
                labR.textContent = t.labelWithEffect + ' (' + effect.toUpperCase() + ')';
                labL.style.color = 'var(--text-dim)';
                descL.textContent = t.descNoEffect;
                descR.textContent = t.descWithEffect;
                statusArea.innerHTML = '<span class="status-badge status-pass">' + t.statusEffect + '</span>';

                if (match.effect_diff) {
                    diffSection.style.display = 'block';
                    diffImg.src = match.effect_diff;
                    dTitle.textContent = t.diffTitleEffect;
                } else {
                    diffSection.style.display = 'none';
                }
            }
        } else {
            wrapper.style.display = 'none';
            statusArea.innerHTML = '';
            emptyMsg.style.display = 'block';
        }
    }

    function setMode(mode) {
        currentComparisonMode = mode;
        btnPR.classList.toggle('active', mode === 'PR');
        btnEffect.classList.toggle('active', mode === 'EFFECT');
        updateDisplay();
    }

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
    const zoomLevel = 2.5;

    function setSliderPos(x) {
        const rect = slider.getBoundingClientRect();
        let pos = ((x - rect.left) / rect.width) * 100;
        pos = Math.max(0, Math.min(100, pos));
        handle.style.left = pos + '%';
        beforeImg.style.clipPath = `inset(0 ${100 - pos}% 0 0)`;
    }

    slider.addEventListener('mousedown', (e) => {
        if (!isZoomActive) {
            isResizing = true;
            setSliderPos(e.pageX);
        }
    });
    window.addEventListener('mouseup', () => isResizing = false);

    window.addEventListener('mousemove', (e) => {
        if (isResizing) setSliderPos(e.pageX);
        if (isZoomActive) moveLens(e);
    });

    // Zoom Logic
    btnZoom.onclick = () => {
        isZoomActive = !isZoomActive;
        btnZoom.classList.toggle('active', isZoomActive);
        zoomIcon.textContent = isZoomActive ? '\uD83D\uDC41\uFE0F\u200D\uD83D\uDDE8\uFE0F' : '\uD83D\uDD0D';
        const t = I18N[currentLang];
        document.getElementById('i18n-zoom-label').textContent = isZoomActive ? t.zoomOff : t.zoomOn;
        lens.style.display = isZoomActive ? 'block' : 'none';

        // Update cursors for all zoomable areas
        document.querySelectorAll('.zoomable').forEach(el => {
            el.style.cursor = isZoomActive ? 'none' : 'crosshair';
        });
    };

    function moveLens(e) {
        const container = e.target.closest('.zoomable');
        if (!container || !isZoomActive) {
            lens.style.display = 'none';
            return;
        }

        const rect = container.getBoundingClientRect();
        const x = e.pageX - rect.left - window.pageXOffset;
        const y = e.pageY - rect.top - window.pageYOffset;

        if (x < 0 || x > rect.width || y < 0 || y > rect.height) {
            lens.style.display = 'none';
            return;
        }

        if (lens.parentElement !== container) {
            container.appendChild(lens);
        }

        lens.style.display = 'block';
        lens.style.left = (x - lens.offsetWidth / 2) + 'px';
        lens.style.top = (y - lens.offsetHeight / 2) + 'px';

        let targetImg;
        if (container.classList.contains('comparison-slider')) {
            const sliderPos = parseFloat(handle.style.left) || 50;
            const xPercent = (x / rect.width) * 100;
            targetImg = (xPercent < sliderPos) ? document.getElementById('refImg') : document.getElementById('actualImg');
        } else {
            targetImg = container.querySelector('img');
        }

        lens.style.backgroundImage = `url("${targetImg.src}")`;
        lens.style.backgroundSize = (rect.width * zoomLevel) + "px " + (rect.height * zoomLevel) + "px";
        lens.style.backgroundPosition = "-" + (x * zoomLevel - lens.offsetWidth / 2) + "px -" + (y * zoomLevel - lens.offsetHeight / 2) + "px";
    }

    document.querySelectorAll('.zoomable').forEach(el => {
        el.addEventListener('mouseenter', () => { if (isZoomActive) lens.style.display = 'block'; });
        el.addEventListener('mouseleave', () => { lens.style.display = 'none'; });
    });

    // Initial calls
    vSel.onchange = updateModes;
    mSel.onchange = updateModes;
    eSel.onchange = updateDisplay;

    applyLang();
    populateViews();
    updateModes();
