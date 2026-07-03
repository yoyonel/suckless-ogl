/* Initialize Mermaid with Dark Theme if Slate palette is active */
document.addEventListener("DOMContentLoaded", function () {
    /* Check for the 'slate' scheme in the body data attribute */
    var isDark = document.body.getAttribute("data-md-color-scheme") === "slate";

    console.log("Mermaid Init: Dark Mode detected = " + isDark);

    mermaid.initialize({
        startOnLoad: true,
        theme: isDark ? 'dark' : 'default',
        securityLevel: 'loose',
        flowchart: {
            useMaxWidth: true,
            htmlLabels: true
        }
    });
});
