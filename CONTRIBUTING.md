# Contributing Guidelines

Thank you for contributing to the Hazardous Gas Early Warning System repository!

---

## Code Style & Guidelines

1. **Follow Existing Style**: Use 4-space indentation, `CamelCase` for class names, `camelCase` for methods, and `m_` prefix for private members.
2. **No AI Boilerplate**: Avoid tutorial-style comments, repetitive inline explanations, or superficial design patterns. Keep comments focused on non-obvious hardware quirks, protocol assumptions, or safety logic reasons.
3. **Keep Files Focused**: Each manager class should reside in its own `.h` / `.cpp` file pair.
4. **Pull Request Validation**:
   - Verify that your changes compile cleanly on ESP32 BSP v2.0+.
   - Document any changes in the commit message or PR description.

---

## Issues & Feature Requests

- Open an issue detailing the hardware setup, steps to reproduce, and serial log outputs.
- For feature requests, explain the safety or operational benefit of the proposed change.
