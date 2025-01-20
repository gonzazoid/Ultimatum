# ![Logo](../chrome/app/theme/chromium/product_logo_64.png) Ultimatum

Note: This README file is intentionally NOT named /.github/README.md to avoid
replacing the root level [README.md](../README.md) on
https://github.com/chromium/chromium. See
[github readme documentation](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-readmes)

## Where is copilot-instructions.md?
[`copilot-instructions.md`](../copilot-instructions.md) is typically a single
instruction file that contains default instructions for a workspace. These
instructions are automatically included in every chat request.

Until the prompt in `copilot-instructions.md` is generally agreed upon for the
chromium repo, this file is intentionally excluded from the repo, and added to
the [.gitignore](../.gitignore) for your customization.

For generating your own `copilot-instructions.md`, type
`/create_copilot_instructions` in GitHub Copilot to get started.

- [.github/instructions](./instructions/): Instructions that are automatically
  picked up using `applyTo` syntax will have a much higher review bar than those
  without it.
- [.github/prompts](./prompts/): All prompts should specify a `mode` and
  `description`.
- [.github/resources](./resources/): All prompt resources should have an active
  reference or use case in a file in `instructions` or `prompts`, and should be
  cleaned up if their references are modified or removed.
