+++
# The root section = the front page. Rendered by templates/index.html.

# `sort_by = "date"` is what puts the combined list in reverse-chronological
# order (Zola sorts dates newest-first). Because content/code/ and
# content/other/ are both `transparent = true`, their posts are handed up to
# this section, so section.pages here is every post from both, already sorted.
sort_by = "date"

# Pagination is off. To turn it on, add `paginate_by = 10` here -- and if you
# have enabled other languages, add it to each content/_index.<lang>.md too,
# since a synthesised root section would not inherit it. templates/index.html
# already handles both cases.
+++
