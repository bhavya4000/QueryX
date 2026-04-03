const themeSwitch = document.getElementById("themeSwitch");
const root = document.documentElement;

themeSwitch.addEventListener("change", () => {
    if (themeSwitch.checked) {
        // Dark Mode
        root.style.setProperty("--background-color", "#353535");
        root.style.setProperty("--text-color", "#d1cece");
    } else {
        // Light Mode
        root.style.setProperty("--background-color", "#fff5f5");
        root.style.setProperty("--text-color", "#353535");
    }
});
