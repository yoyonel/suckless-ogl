# Documentation Staging & Preview Guide

This project automatically generates and deploys a live preview of the Doxygen documentation for every Pull Request. This allows for functional validation of documentation changes before merging.

## How it Works

1.  **Trigger**: Every push to a Pull Request triggers the `documentation` job in GitHub Actions.
2.  **Generation**: Doxygen builds the HTML documentation.
3.  **Deployment**: The documentation is deployed to [Surge.sh](https://surge.sh) using a unique domain per PR: `suckless-ogl-pr-[NUMBER].surge.sh`.
4.  **Feedback**: A comment is automatically posted on the Pull Request with a link to the live preview.

## Setup Instructions

To enable automated deployments, you must configure a `SURGE_TOKEN` in your repository secrets.

### 1. Obtain a Surge Token

If you haven't used Surge before, you can install it and generate a token via CLI:

```bash
npx surge token
```

- If you don't have an account, it will prompt you to create one.
- Copy the provided token string.

### 2. Configure GitHub Secrets

1. Navigate to your GitHub repository.
2. Go to **Settings** > **Secrets and variables** > **Actions**.
3. Create a **New repository secret**.
4. Set the **Name** to `SURGE_TOKEN`.
5. Paste your token into the **Value** field.

## Workflow Integration

The staging logic is defined in `.github/workflows/main.yml`. It uses the `mshick/add-pr-comment` action to keep you informed:

```yaml
- name: Comment Preview Link on PR
  if: github.event_name == 'pull_request'
  uses: mshick/add-pr-comment@v2
  with:
    message: |
      ## 📚 Documentation Preview
      The Doxygen documentation for this PR has been generated and is ready for review:
      🔗 **[Preview Link](${{ env.PREVIEW_URL }})**
```

## Maintenance

### Clearing Previews
Surge.sh keeps the deployments indefinitely. Since the domains are reused per PR number, new pushes simply overwrite the previous version. If you need to manually tear down a preview:

```bash
npx surge teardown suckless-ogl-pr-[NUMBER].surge.sh --token your_token
```

### Security
The `SURGE_TOKEN` allows anyone with access to it to deploy sites to your Surge account. Keep it secret and only store it in GitHub Secrets.
