# biart.github.io

A minimal personal blog built with [Zola](https://www.getzola.org/).

---

## Switching domain

1. Change `base_url` in `zola.toml` to `https://your-domain.tld` and push.
2. Repo **Settings → Pages → Custom domain**: enter the same domain, save.
3. DNS: for a subdomain, `CNAME` → `biart.github.io`. For an apex domain, four
   `A` records to GitHub's Pages IPs (currently `185.199.108–111.153`) or an
   `ALIAS`/`ANAME` if your registrar supports it.
4. Tick **Enforce HTTPS** once the certificate is issued.

Step 1 is the only change in this repo. Links, feeds, `sitemap.xml`,
`robots.txt`, and the generated CNAME all follow.

## Adding a language

1. Uncomment its `[languages.xx]` block **and** its
   `[languages.xx.translations]` table in `zola.toml`.
2. Add a translated `_index` for the root and for each section:
   `content/_index.xx.md`, `content/code/_index.xx.md`,
   `content/other/_index.xx.md`. This is not optional — the navigation calls
   `get_url(path="@/code/_index.md", lang=lang)`, which fails the build if
   `_index.xx.md` does not exist. Copy the front matter from the English file
   and translate `title`/`description`.
3. Translate posts by adding `index.xx.md` next to an existing `index.md`.
   Untranslated posts simply do not appear in that language.
4. If you have turned pagination on, add `paginate_by` to each
   `content/_index.xx.md` too.

## Images: commit one shrunk original, generate the rest

`{{ figure(src="...", alt="...") }}` (see `templates/shortcodes/figure.html`)
reads the original's real dimensions with `get_image_metadata`, then calls
`resize_image` for the 480/960/1440px variants and emits a `srcset` plus
`width`/`height`/`loading="lazy"`.

Images are *colocated*: they live in the post's own directory next to its
`index.md`, so moving or deleting a post takes its images with it. This is why
both posts are directories with an `index.md` rather than flat `.md` files —
a flat file works identically until the day it needs an image, at which point
you have to restructure it.

The **no-upscale guard** matters: variants wider than the original are skipped,
so a 1200px original yields 480/960/1200 and no blurry 1440. The demo image is
1200px wide specifically to exercise that path.

Nothing generated is committed, so the repo grows only by the size of your
originals — which is why this needs no Git LFS. Keep originals at roughly 1440px
unless you specifically want something larger available.

## Deployment

`.github/workflows/deploy.yml`, on push to `main`, plus manual
`workflow_dispatch`.

**One-time setup:** repo **Settings → Pages → Source: GitHub Actions**. The
workflow runs without it but the deploy step fails.

It downloads a pinned Zola release and uses GitHub's own `configure-pages`,
`upload-pages-artifact` and `deploy-pages` actions. `ZOLA_VERSION` is pinned on
purpose — unpinned, a future Zola release could change your output with no
commit from you. Keep it in step with your local `zola --version`.

## Smaller choices

- Multilingual is wired up.
- Atom feed.
- `minify_html = true`. It is `<pre>`-aware, so code blocks are unaffected. It
  does unquote and reorder attributes, which makes `public/` awkward to read —
  set it to `false` while debugging generated HTML.
- `insert_anchor_links = "heading"` makes every heading a link to itself, with
  no extra markup or CSS needed.
- `smart_punctuation = true`, `bottom_footnotes = true`.
- External links get `target="_blank"` + `rel="noreferrer"`.
- `build_search_index = false` — a search index needs client-side JavaScript to
  be useful, which this site does not have.
- No taxonomies (tags). `zola.toml` has a verified snippet if you want them;
  note they also need `templates/tags/list.html` and `templates/tags/single.html`
  or the build fails.
- A skip-link, focus-visible outlines and intrinsic image dimensions are in
  there because they are much easier to include now than to retrofit.
