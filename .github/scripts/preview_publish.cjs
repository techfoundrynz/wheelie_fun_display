const fs = require('node:fs');
const path = require('node:path');
const SOURCE = '.cache/incoming-site';
const MARKER = '<!-- wheelie-display-preview -->';
const read = (source) => JSON.parse(fs.readFileSync(path.join(source, 'preview.json'), 'utf8'));

async function locate({github, context, core}) {
  if (context.eventName === 'pull_request_target') return;
  const artifacts = await github.paginate(github.rest.actions.listWorkflowRunArtifacts, {
    ...context.repo, run_id: context.payload.workflow_run.id, per_page: 100,
  });
  const artifact = artifacts.find(a => a.name === 'wheelie-fun-display-pages' && !a.expired);
  core.setOutput('available', Boolean(artifact));
  if (artifact) {
    if (!Number.isSafeInteger(artifact.id) || artifact.id < 1) throw new Error('Invalid artifact ID');
    core.setOutput('artifact_id', artifact.id);
  }
}

async function validate({github, context, core, source = SOURCE}) {
  if (context.eventName === 'pull_request_target') {
    const number = context.payload.pull_request.number;
    if (!Number.isSafeInteger(number) || number < 1) throw new Error('Invalid PR number');
    core.setOutput('publish', true);
    core.setOutput('destination', `previews/pr-${number}`);
    core.setOutput('remove', true);
    return;
  }
  const metadata = read(source);
  const run = context.payload.workflow_run;
  if (run.conclusion !== 'success') throw new Error('Build did not succeed');
  if (metadata.repository !== `${context.repo.owner}/${context.repo.repo}` || metadata.run_id !== run.id ||
      !/^[a-f0-9]{40}$/.test(metadata.commit) || metadata.commit !== run.head_sha) {
    throw new Error('Preview does not match its originating build');
  }
  if (metadata.kind === 'pr') {
    if (run.event !== 'pull_request' || !Number.isSafeInteger(metadata.pr) || metadata.pr < 1) throw new Error('Invalid PR provenance');
    const pr = (await github.rest.pulls.get({...context.repo, pull_number: metadata.pr})).data;
    if (pr.base.repo.full_name !== metadata.repository || pr.head.sha !== metadata.commit || pr.state !== 'open') {
      core.info('Skipping closed or superseded preview');
      return;
    }
    core.setOutput('destination', `previews/pr-${pr.number}/${metadata.commit}`);
  } else if (metadata.kind !== 'site' || !['push', 'workflow_dispatch'].includes(run.event)) {
    throw new Error('Only trusted branch/tag runs may replace the production site');
  }
  core.setOutput('source', source);
  core.setOutput('publish', true);
}

async function report({github, context, core, source = SOURCE}) {
  const closed = context.eventName === 'pull_request_target';
  const metadata = closed ? {pr: context.payload.pull_request.number} : read(source);
  const base = (process.env.PAGES_URL || '').replace(/\/$/, '');
  const destination = (process.env.PREVIEW_DESTINATION || '').split('/').map(encodeURIComponent).join('/');
  const url = `${base}/${destination}/`;
  const body = closed ? `${MARKER}\nSimulator preview removed because this PR is closed.` : metadata.kind === 'pr'
    ? `${MARKER}\n### Wheelie Fun Display preview\n[Open interactive simulator](${url}) · [Browser test gallery](${url}report/)\n\nCommit \`${metadata.commit.slice(0, 8)}\`. Validation passed. Preview code runs in an isolated frame with hardware access disabled.`
    : `### Wheelie Fun Display\n[Documentation](${base}/) · [Interactive simulator](${base}/simulator/) · [Firmware installer](${base}/install/)`;
  await core.summary.addRaw(body).write();
  if (!closed && metadata.kind !== 'pr') return;
  const comments = await github.paginate(github.rest.issues.listComments, {...context.repo, issue_number: metadata.pr, per_page: 100});
  const previous = comments.find(c => c.user.login === 'github-actions[bot]' && (c.body || '').includes(MARKER));
  if (previous) await github.rest.issues.updateComment({...context.repo, comment_id: previous.id, body});
  else if (!closed) await github.rest.issues.createComment({...context.repo, issue_number: metadata.pr, body});
}

module.exports = {locate, validate, report};
