# The prototype engines

`engine-v2.js` and `engine-v1.js` are the JavaScript engines of the web prototype that Kindred was
ported from. They are the reference of the golden test: `../gen-fixtures.js` runs them with fixed
seeds, and `../GoldenTest.cpp` replays the same operations in the C++ engine and compares every
result. `../../../tools/make-style.js` reads the tables of `engine-v2.js` to write `styles/*.json`.

Do not change their behavior. They are copies; one comment line of `engine-v2.js` was reworded.
The project files of the prototype call the "Major modes only" switch `officeSafe` and the
"Extended colors" switch `jazz`; Kindred reads those names when it opens an old file.
