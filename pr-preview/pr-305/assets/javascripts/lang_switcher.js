document.addEventListener("DOMContentLoaded", function () {
  const lang = (document.documentElement.lang || "en").toUpperCase().slice(0, 2);
  const btn = document.querySelector(
    ".md-header__option .md-select > button.md-header__button"
  );
  if (btn) btn.setAttribute("data-lang", lang);
});
