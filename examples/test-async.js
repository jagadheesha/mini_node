console.log("1: Main script starts");

setTimeout(() => {
    console.log("3: Timer callback executed");
}, 50);

mini.readFile("./package.json", (err, data) => {
    if (err) console.log("File error:", err);
    else console.log("4: Async file I/O finished, bytes:", data.length);
});

console.log("2: Main script finishes, entering event loop");