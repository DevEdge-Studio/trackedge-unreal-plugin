# Contributing to TrackEdge

Thank you for helping improve the TrackEdge Unreal Engine plugin.

## Before you start

- Read the [`LICENSE`](LICENSE) before using or contributing code.
- Keep contributions focused on the plugin and its integration with the TrackEdge product.
- Do not submit code, assets, credentials, customer data, or proprietary material owned by another party.
- Do not include API keys, project IDs, private URLs, generated build output, or local Unreal project files in a pull request.

## Contribution rules

By contributing to this repository, you agree that:

1. Your contribution is your original work, or you have permission to submit it.
2. You grant DevEdge Studio the rights needed to use, modify, distribute, and relicense your contribution as part of TrackEdge.
3. Your contribution does not copy code from a third-party project unless its license is compatible and is clearly documented.
4. You will preserve the copyright and license notices required by the MIT License.
5. You will not imply endorsement by DevEdge Studio or misuse TrackEdge trademarks and branding.

## Development guidelines

- Target Unreal Engine 5.7 unless a change specifically requires another version.
- Preserve the separation between the runtime `TrackEdge` module and the editor-only `TrackEdgeEditor` module.
- Keep runtime code safe for packaged builds; editor-only dependencies must remain in `TrackEdgeEditor`.
- Use Unreal naming, reflection, module, and formatting conventions already present in the project.
- Keep HTTP operations asynchronous and avoid blocking the game thread.
- Update `README.md` when adding or changing user-facing features, settings, Blueprint nodes, or installation steps.

## Pull requests

Please include:

- A short description of the problem and the proposed change.
- The Unreal Engine version used for testing.
- Testing steps and relevant Output Log results.
- Screenshots or recordings for editor UI changes.
- Any configuration, API, network, or licensing implications.

Small focused pull requests are easier to review. Maintainers may request changes, reject changes that conflict with the product roadmap, or ask for a contributor license clarification before merging.

## Reporting security issues

Do not publish API keys, credentials, exploitable vulnerabilities, or private customer data in an issue. Contact the project maintainers privately through the support channel listed in the README.
