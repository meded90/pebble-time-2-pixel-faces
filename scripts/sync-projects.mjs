#!/usr/bin/env node

import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {existsSync, readFileSync, writeFileSync} from 'node:fs';
import {dirname, relative, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const checkOnly = process.argv.includes('--check');
const catalog = JSON.parse(readFileSync(resolve(root, 'projects.json'), 'utf8'));
const projects = catalog.projects;
const changed = [];

function fail(message) {
  throw new Error(message);
}

function assertFile(path, label) {
  if (!existsSync(resolve(root, path))) fail(`${label} does not exist: ${path}`);
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function validateChangelog(project, version) {
  const path = `${project.directory}/CHANGELOG.md`;
  assertFile(path, `${project.id} changelog`);
  const content = readFileSync(resolve(root, path), 'utf8');
  const heading = new RegExp(
    `^##[ \\t]+\\[?${escapeRegExp(version)}\\]?(?:[ \\t]+[-–—][ \\t]+.*)?[ \\t]*$`,
    'm'
  ).exec(content);
  if (!heading) {
    fail(`${project.id}: CHANGELOG.md has no entry for source version ${version}`);
  }
  const sectionStart = heading.index + heading[0].length;
  const remaining = content.slice(sectionStart);
  const nextHeading = remaining.search(/^##[ \\t]+/m);
  const section = nextHeading < 0 ? remaining : remaining.slice(0, nextHeading);
  if (!/^[ \t]*[-*][ \t]+\S/m.test(section)) {
    fail(`${project.id}: changelog entry ${version} needs at least one release-note bullet`);
  }
}

function writeGenerated(path, content) {
  const absolute = resolve(root, path);
  const normalized = `${content.trimEnd()}\n`;
  const current = existsSync(absolute) ? readFileSync(absolute, 'utf8') : '';
  if (current === normalized) return;
  changed.push(path);
  if (!checkOnly) writeFileSync(absolute, normalized);
}

function replaceBlock(path, start, end, content) {
  const absolute = resolve(root, path);
  const current = readFileSync(absolute, 'utf8');
  const startIndex = current.indexOf(start);
  const endIndex = current.indexOf(end);
  if (startIndex < 0 || endIndex < 0 || endIndex < startIndex) {
    fail(`Generated markers are missing or out of order in ${path}`);
  }
  const before = current.slice(0, startIndex + start.length);
  const after = current.slice(endIndex);
  const next = `${before}\n${content.trim()}\n${after}`;
  if (next === current) return;
  changed.push(path);
  if (!checkOnly) writeFileSync(absolute, next);
}

function chunk(items, size) {
  const rows = [];
  for (let index = 0; index < items.length; index += size) {
    rows.push(items.slice(index, index + size));
  }
  return rows;
}

function link(project) {
  return `[${project.name}](${project.directory}/)`;
}

function storeStatus(project, language) {
  if (project.store.status === 'published') {
    const label = language === 'en' ? 'Install from RePebble' : 'Установить из RePebble';
    return `[${label}](${project.store.url})`;
  }
  if (project.store.status === 'experimental') {
    return language === 'en' ? 'Experimental · Not published' : 'Эксперимент · Не опубликован';
  }
  return language === 'en' ? 'Release candidate · Not published' : 'Релиз-кандидат · Не опубликован';
}

function gallery(projectList, language) {
  return chunk(projectList, 4).map((group) => {
    const titles = `| ${group.map(link).join(' | ')} |`;
    const separator = `| ${group.map(() => '---').join(' | ')} |`;
    const previews = `| ${group.map((project) => `[![${project.name}](${project.preview})](${project.directory}/)`).join(' | ')} |`;
    const statuses = `| ${group.map((project) => storeStatus(project, language)).join(' | ')} |`;
    return [titles, separator, previews, statuses].join('\n');
  }).join('\n\n');
}

function catalogSection(language) {
  const isEnglish = language === 'en';
  const watchfaces = projects.filter((project) => project.type === 'watchface');
  const watchapps = projects.filter((project) => project.type === 'watchapp');
  const lines = [
    `## ${isEnglish ? 'Project gallery' : 'Галерея проектов'}`,
    '',
    `### ${isEnglish ? 'Watchfaces' : 'Циферблаты'}`,
    '',
    gallery(watchfaces, language),
    '',
    `### ${isEnglish ? 'Watchapps' : 'Приложения'}`,
    '',
    gallery(watchapps, language),
    '',
    isEnglish
      ? '“Not published” means that the repository contains source code or a release candidate, but no verified public Appstore page. It does not mean the project has passed final hardware or service validation.'
      : '«Не опубликован» означает, что в репозитории есть исходники или релиз-кандидат, но нет проверенной публичной страницы в Appstore. Это не означает, что проект прошёл финальную проверку на физических часах или с реальным внешним сервисом.',
    '',
    `## ${isEnglish ? 'What each project does' : 'Назначение проектов'}`,
    '',
    isEnglish ? '| Project | Type | Source version | Purpose |' : '| Проект | Тип | Версия исходников | Назначение |',
    '| --- | --- | --- | --- |',
    ...projects.map((project) => {
      const type = isEnglish
        ? (project.type === 'watchface' ? 'Watchface' : 'Watchapp')
        : (project.type === 'watchface' ? 'Циферблат' : 'Приложение');
      return `| ${link(project)} | ${type} | ${project.sourceVersion} | ${project.description[language]} |`;
    })
  ];
  return lines.join('\n');
}

function publishedSection(language) {
  return projects
    .filter((project) => project.store.status === 'published')
    .map((project) => `- [${project.name}](${project.store.url}) — ${project.store.publishedVersion}`)
    .join('\n');
}

function distReadme(language) {
  const isEnglish = language === 'en';
  const statusLabel = {
    published: isEnglish ? 'Published-version binary' : 'Бинарник опубликованной версии',
    candidate: isEnglish ? 'Release candidate' : 'Релиз-кандидат',
    experimental: isEnglish ? 'Experimental prototype' : 'Экспериментальный прототип'
  };
  const rows = [...projects]
    .sort((left, right) => left.artifact.path.localeCompare(right.artifact.path))
    .map((project) => {
      const artifactLink = relative(resolve(root, 'dist'), resolve(root, project.artifact.path));
      const store = project.store.status === 'published'
        ? `[${project.store.publishedVersion}](${project.store.url})`
        : (isEnglish ? 'Not published' : 'Не опубликован');
      return `| [${project.name}](../${project.directory}/) | [${artifactLink}](${artifactLink}) | ${project.artifact.version} | ${statusLabel[project.artifact.status]} | ${store} |`;
    });
  return [
    '# Release artifacts',
    '',
    isEnglish ? '**English** · [Русский](README.ru.md)' : '[English](README.md) · **Русский**',
    '',
    isEnglish
      ? 'PBW files are separated by the status of the binary itself. “Published-version” means that `versionLabel` matches a verified public release; byte identity with the Appstore download is not implied unless separately documented. A newer local binary remains a release candidate.'
      : 'PBW разделены по статусу самого бинарника. «Опубликованная версия» означает совпадение `versionLabel` с проверенным публичным релизом; идентичность байтов со скачиванием Appstore без отдельной проверки не заявляется. Более новая локальная сборка остаётся релиз-кандидатом.',
    '',
    isEnglish ? '| Project | Artifact | Version | Binary status | Store status |' : '| Проект | Артефакт | Версия | Статус бинарника | Статус в магазине |',
    '| --- | --- | --- | --- | --- |',
    ...rows,
    '',
    isEnglish
      ? '`experimental/voice-drop-prototype-0.1.0.pbw` is the legacy phone-recording prototype and is not compatible with the current Voice Drop 0.2.0 source.'
      : '`experimental/voice-drop-prototype-0.1.0.pbw` — старый прототип записи через телефон, несовместимый с текущими исходниками Voice Drop 0.2.0.',
    '',
    isEnglish
      ? 'Verify every file against [SHA256SUMS](SHA256SUMS). Regenerate this directory metadata with `npm run projects:sync`.'
      : 'Проверяйте каждый файл по [SHA256SUMS](SHA256SUMS). Для обновления метаданных каталога выполните `npm run projects:sync`.'
  ].join('\n');
}

if (catalog.schemaVersion !== 1 || !Array.isArray(projects)) fail('Unsupported projects.json schema');
const ids = new Set();
const uuids = new Set();
for (const project of projects) {
  if (ids.has(project.id)) fail(`Duplicate project id: ${project.id}`);
  ids.add(project.id);
  assertFile(project.directory, 'Project directory');
  assertFile(project.preview, 'Preview');
  assertFile(project.artifact.path, 'Release artifact');
  const packageJson = JSON.parse(readFileSync(resolve(root, project.directory, 'package.json'), 'utf8'));
  if (packageJson.name !== project.id) {
    fail(`${project.id}: package.json name is ${packageJson.name}`);
  }
  validateChangelog(project, packageJson.version);
  if (packageJson.version !== project.sourceVersion) {
    fail(`${project.id}: projects.json version ${project.sourceVersion} does not match package.json ${packageJson.version}`);
  }
  const uuid = packageJson.pebble?.uuid;
  if (!uuid || uuids.has(uuid)) fail(`${project.id}: missing or duplicate Pebble UUID ${uuid}`);
  uuids.add(uuid);
  const isWatchface = packageJson.pebble?.watchapp?.watchface === true;
  if (isWatchface !== (project.type === 'watchface')) {
    fail(`${project.id}: project type does not match package.json watchface metadata`);
  }
  const packagePlatforms = [...(packageJson.pebble?.targetPlatforms ?? [])].sort();
  const catalogPlatforms = [...project.platforms].sort();
  if (JSON.stringify(packagePlatforms) !== JSON.stringify(catalogPlatforms)) {
    fail(`${project.id}: platform list does not match package.json`);
  }
  const preview = readFileSync(resolve(root, project.preview));
  if (preview.toString('ascii', 1, 4) !== 'PNG') fail(`${project.id}: preview is not a PNG`);
  const width = preview.readUInt32BE(16);
  const height = preview.readUInt32BE(20);
  if (width !== 200 || height !== 228) {
    fail(`${project.id}: catalog preview must be a native Emery 200x228 PNG, got ${width}x${height}`);
  }
  if (project.store.status === 'published' && (!project.store.url || !project.store.publishedVersion)) {
    fail(`${project.id}: a published project needs url and publishedVersion`);
  }
  const appInfo = JSON.parse(execFileSync('unzip', ['-p', resolve(root, project.artifact.path), 'appinfo.json'], {encoding: 'utf8'}));
  if (appInfo.versionLabel !== project.artifact.version) {
    fail(`${project.id}: artifact version ${appInfo.versionLabel} does not match catalog ${project.artifact.version}`);
  }
  const expectedDirectory = project.artifact.status === 'candidate' ? 'candidates' : project.artifact.status;
  if (!project.artifact.path.startsWith(`dist/${expectedDirectory}/`)) {
    fail(`${project.id}: ${project.artifact.status} artifact is in the wrong directory`);
  }
}

const faces = projects.filter((project) => project.buildGroup === 'faces').map((project) => project.directory);
const standardApps = projects.filter((project) => project.buildGroup === 'standard-apps').map((project) => project.directory);
const patchedApps = projects.filter((project) => project.buildGroup === 'patched-apps').map((project) => project.directory);
const makeVariables = [
  `FACES := ${faces.join(' ')}`,
  `STANDARD_APPS := ${standardApps.join(' ')}`,
  `PATCHED_APPS := ${patchedApps.join(' ')}`
].join('\n');

replaceBlock('Makefile', '# projects:make:start', '# projects:make:end', makeVariables);
replaceBlock('README.md', '<!-- projects:catalog:start -->', '<!-- projects:catalog:end -->', catalogSection('en'));
replaceBlock('README.ru.md', '<!-- projects:catalog:start -->', '<!-- projects:catalog:end -->', catalogSection('ru'));
replaceBlock('README.md', '<!-- projects:published:start -->', '<!-- projects:published:end -->', publishedSection('en'));
replaceBlock('README.ru.md', '<!-- projects:published:start -->', '<!-- projects:published:end -->', publishedSection('ru'));
writeGenerated('dist/README.md', distReadme('en'));
writeGenerated('dist/README.ru.md', distReadme('ru'));

const checksums = [...projects]
  .sort((left, right) => left.artifact.path.localeCompare(right.artifact.path))
  .map((project) => {
    const bytes = readFileSync(resolve(root, project.artifact.path));
    const hash = createHash('sha256').update(bytes).digest('hex');
    return `${hash}  ${relative(resolve(root, 'dist'), resolve(root, project.artifact.path))}`;
  })
  .join('\n');
writeGenerated('dist/SHA256SUMS', checksums);

const uniqueChanged = [...new Set(changed)];

if (checkOnly && uniqueChanged.length > 0) {
  console.error(`Generated files are stale: ${uniqueChanged.join(', ')}`);
  process.exit(1);
}

console.log(uniqueChanged.length === 0 ? 'Project catalog is up to date.' : `Updated: ${uniqueChanged.join(', ')}`);
