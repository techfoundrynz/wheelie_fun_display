const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const publish = require('../.github/scripts/preview_publish.cjs');

function fixture(t) {
  const source = fs.mkdtempSync(path.join(os.tmpdir(), 'wfd-preview-test-'));
  t.after(() => fs.rmSync(source, {recursive: true, force: true}));
  const metadata = {kind: 'pr', commit: 'a'.repeat(40), pr: 12, repository: 'owner/display', run_id: 31};
  const context = {eventName: 'workflow_run', repo: {owner: 'owner', repo: 'display'}, payload: {
    workflow_run: {id: 31, event: 'pull_request', head_sha: metadata.commit, conclusion: 'success'},
  }};
  const pr = {number: 12, state: 'open', head: {sha: metadata.commit}, base: {repo: {full_name: metadata.repository}}};
  const outputs = {};
  const calls = [];
  const core = {setOutput: (name, value) => { outputs[name] = value; }, info: () => {},
    summary: {addRaw: body => { calls.push(['summary', body]); return {write: async () => {}}; }}};
  const github = {rest: {pulls: {get: async () => ({data: pr})},
    actions: {listWorkflowRunArtifacts: 'artifacts'}, issues: {listComments: 'comments',
      createComment: async value => calls.push(['create', value]),
      updateComment: async value => calls.push(['update', value])}},
    paginate: async kind => kind === 'comments' ? [] : [{id: 101, name: 'wheelie-fun-display-pages', expired: false}]};
  const write = () => fs.writeFileSync(path.join(source, 'preview.json'), JSON.stringify(metadata));
  write();
  return {source, metadata, context, pr, core, github, outputs, calls, write};
}

test('a successful current PR produces a revision-specific destination', async t => {
  const f = fixture(t);
  await publish.validate(f);
  assert.equal(f.outputs.publish, true);
  assert.equal(f.outputs.destination, `previews/pr-12/${f.metadata.commit}`);
});

test('superseded or closed PRs are not published', async t => {
  for (const kind of ['superseded', 'closed']) {
    const f = fixture(t);
    if (kind === 'closed') f.pr.state = 'closed'; else f.pr.head.sha = 'b'.repeat(40);
    await publish.validate(f);
    assert.equal(f.outputs.publish, undefined);
  }
});

test('PR artifacts cannot replace production or claim another run/repository', async t => {
  for (const field of ['kind', 'commit', 'run_id', 'repository', 'pr']) {
    const f = fixture(t);
    f.metadata[field] = {kind: 'site', commit: 'b'.repeat(40), run_id: 32, repository: 'elsewhere/repo', pr: '../root'}[field];
    f.write();
    await assert.rejects(publish.validate(f));
  }
});

test('closed PR cleanup has a narrowly scoped path and needs no artifact', async t => {
  const f = fixture(t);
  f.context.eventName = 'pull_request_target';
  f.context.payload.pull_request = {number: 12};
  fs.unlinkSync(path.join(f.source, 'preview.json'));
  await publish.validate(f);
  assert.equal(f.outputs.destination, 'previews/pr-12');
  assert.equal(f.outputs.remove, true);
});

test('production artifacts require a non-PR successful build', async t => {
  const f = fixture(t);
  f.metadata.kind = 'site';
  f.context.payload.workflow_run.event = 'push';
  f.write();
  await publish.validate(f);
  assert.equal(f.outputs.publish, true);
  assert.equal(f.outputs.destination, undefined);
});

test('published preview links update the existing bot comment', async t => {
  const f = fixture(t);
  f.github.paginate = async () => [{id: 42, user: {login: 'github-actions[bot]'}, body: '<!-- wheelie-display-preview --> old'}];
  const old = {url: process.env.PAGES_URL, destination: process.env.PREVIEW_DESTINATION};
  process.env.PAGES_URL = 'https://owner.github.io/display/';
  process.env.PREVIEW_DESTINATION = `previews/pr-12/${f.metadata.commit}`;
  t.after(() => {
    if (old.url === undefined) delete process.env.PAGES_URL; else process.env.PAGES_URL = old.url;
    if (old.destination === undefined) delete process.env.PREVIEW_DESTINATION; else process.env.PREVIEW_DESTINATION = old.destination;
  });
  await publish.report(f);
  const update = f.calls.find(call => call[0] === 'update')[1];
  assert.equal(update.comment_id, 42);
  assert.ok(update.body.includes(`https://owner.github.io/display/previews/pr-12/${f.metadata.commit}/`));
  assert.ok(update.body.includes('report/'));
  assert.ok(!f.calls.some(call => call[0] === 'create'));
});

test('only a live expected artifact is selected', async t => {
  const f = fixture(t);
  await publish.locate(f);
  assert.equal(f.outputs.available, true);
  assert.equal(f.outputs.artifact_id, 101);
  f.github.paginate = async () => [{id: 101, name: 'wheelie-fun-display-pages', expired: true}];
  await publish.locate(f);
  assert.equal(f.outputs.available, false);
});
