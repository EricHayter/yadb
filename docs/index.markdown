---
# Feel free to add content and custom Front Matter to this file.
# To modify the layout, see https://jekyllrb.com/docs/themes/#overriding-theme-defaults

layout: home
---


Welcome to the Yet Another Database Homepage

{% assign date = '2025-05-29T15:30:00Z' %}

- Original date - {{ date }}
- With timeago filter - {{ date | timeago }}
