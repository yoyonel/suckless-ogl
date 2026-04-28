# Documentation Staging & Preview Guide

This project automatically generates and deploys a live preview of the documentation for every Pull Request. This allows for functional validation of documentation changes before merging.

## How it Works

1. **Trigger**: Every push to a Pull Request triggers the `documentation` job in GitHub Actions.
2. **Generation**: MkDocs + Doxygen build the full documentation site.
3. **Deployment**: The documentation is deployed to [GitHub Pages](https://pages.github.com/) as a subdirectory under `pr-preview/pr-<NUMBER>/` using [`rossjrw/pr-preview-action`](https://github.com/rossjrw/pr-preview-action).
4. **Feedback**: A comment is automatically posted on the Pull Request with a link to the live preview.
5. **Cleanup**: When the PR is closed or merged, the preview is automatically removed.

## Preview URL Format

```text
https://yoyonel.github.io/suckless-ogl/pr-preview/pr-<NUMBER>/
```

For example, PR #42 would be available at:
`https://yoyonel.github.io/suckless-ogl/pr-preview/pr-42/`

## Setup Instructions

No external tokens or services are required. The deployment uses the built-in `GITHUB_TOKEN` provided by GitHub Actions.

### Repository Configuration

1. Navigate to your GitHub repository **Settings** > **Pages**.
2. Ensure the source is set to **Deploy from a branch** (`gh-pages`).
3. In **Settings** > **Actions** > **General** > **Workflow permissions**, select **Read and write permissions**.

## Workflow Integration

The preview logic is split into two jobs in `.github/workflows/main.yml`:

### PR Preview (deploy-preview job)

```yaml
deploy-preview:
  needs: [documentation]
  if: github.event_name == 'pull_request' && always()
  runs-on: ubuntu-latest
  concurrency: preview-${{ github.ref }}
  steps:
    - uses: actions/checkout@v6
    - uses: actions/download-artifact@v8
      if: github.event.action != 'closed'
      with:
        name: doxygen-docs
        path: preview-site
    - uses: rossjrw/pr-preview-action@v1
      with:
        source-dir: ./preview-site/
        preview-branch: gh-pages
        umbrella-dir: pr-preview
        action: auto
        comment: true
```

### Production Deployment (deploy-pages job)

The production deployment on `master` uses `JamesIves/github-pages-deploy-action` with `clean-exclude: pr-preview` to preserve active PR previews:

```yaml
- uses: JamesIves/github-pages-deploy-action@v4
  with:
    branch: gh-pages
    folder: ./public-site
    clean-exclude: pr-preview
    force: false
```

## Preview Content

The preview includes the **full documentation site**:

- MkDocs documentation (architecture, guides, API)
- Doxygen API reference
- Coverage reports

## Maintenance

### Automatic Cleanup

Previews are automatically removed when a PR is closed or merged. The `pull_request: closed` event triggers the `deploy-preview` job with `action: auto`, which detects the close and removes the `pr-preview/pr-<N>/` directory from `gh-pages`.

### Manual Cleanup

If a preview was not properly cleaned up, you can remove it manually:

```bash
git checkout gh-pages
rm -rf pr-preview/pr-<NUMBER>
git add -A && git commit -m "chore: remove stale PR preview"
git push origin gh-pages
```

## See Also

- [cicd_pipeline.md](./cicd_pipeline.md) — Full CI/CD pipeline overview
- [documentation_system.md](./documentation_system.md) — Documentation system overview
