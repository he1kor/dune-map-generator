UI descriptions are defined in `template_editor_descriptions.json`: parameter
tooltips, canvas hints, notifications, help sections and map size explanations.
Edit the `parameters` and `text` dictionaries or the `help` section array.
Named placeholders such as `{id}` and `{count}` are replaced literally; `%` is
ordinary text. Menu labels and validation errors remain in the application.

Keep descriptions focused on what a control changes. Tooltips should explain
the effect and any important constraint in one or two sentences. Notifications
report the result without suggesting a next step. Keep detailed canvas controls
in Help and shortcuts beside their actions. Use zone and connection consistently
in display text; saved JSON keys keep their existing names. An empty default
description omits the generic help marker.

The `materials` dictionary maps saved resource keys to their display names, for
example `"cliff_rock": "Cliff rock"`. These names appear in material pickers;
saved templates continue to use the canonical keys. Runtime overrides work for
material names in the same way as descriptions.

CMake embeds this same JSON as a fallback and copies it beside the executable.
You can edit that adjacent `template_editor_descriptions.json` and restart the
app without rebuilding. Partial overrides are supported; omitted keys retain
their defaults. Invalid files leave the bundled descriptions intact and report
the error in Help. CMake configuration restores the adjacent copy from this
source asset, so keep permanent changes here.
