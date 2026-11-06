# AI-assisted development

AI development tools such as Claude Code, GitHub Copilot and similar tools are allowed in RCBBoard. (This replaces the AI policy inherited from HeliBoard, which discouraged AI use.)

If you use them:

1. **You are responsible for your changes**, whether you wrote them or an AI tool did.
2. **Understand, review and test** AI-generated code before submitting it, like any other code (see [CONTRIBUTING.md](CONTRIBUTING.md#testing)).
3. **Never give secrets or private data to AI tools**: no API keys, passwords, signing keys or keystores, `local.properties`, personal data, or user data such as dictionaries, typing history or clipboard contents.
4. **Be extra careful with security-sensitive code** (native code, file import, backup and restore, clipboard, permissions, anything that parses external data): review it closely and add tests.
5. **Keep it offline and GPL-3.0 compatible**: AI-generated code must not add network access, the `INTERNET` permission, analytics or new permissions, and must not copy code under an incompatible licence.
6. It is nice to mention in a pull request when you used AI tools substantially.
