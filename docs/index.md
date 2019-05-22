---
layout: default
---
## Modules
{% for item in site.data.api %}
### <code>{{ item.name }}</code>
{{ item.desc }}
{% endfor %}

## Enums
<table>
    <tbody>
{% for module in site.data.api %}
    {% for item in module.members %}
        {% if item.type contains 'number' %}
        <tr>
            <td><strong>{{ module.name }}.{{ item.name }}</strong></td>
            <td>{{ item.desc }}</td>
        </tr>

        {% endif %}
    {% endfor %}
{% endfor %}
    </tbody>
</table>

<hr>

## Functions
<table>
    <tbody>
{% for module in site.data.api %}
    {% for item in module.members %}
        {% if item.type contains 'function' %}
        <tr>
            <td><a href="#{{ item.name | url_encode }}"><strong>{{ module.name }}.{{ item.name }}</strong></a></td>
            <td>{{ item.desc | truncate: 80 }}</td>
        </tr>
        {% endif %}
    {% endfor %}
{% endfor %}
    </tbody>
</table>

{% for module in site.data.api %}
    {% for function in module.members %}
        {% if function.type contains 'function' %}
<div class="function-wrap">
<h3 class="function-header"><a href="#{{ function.name | url_encode }}" id="{{ function.name | url_encode }}"><code>{{ module.name }}.{{ function.name }}({% for param in function.parameters %}{{param.name}}{% unless forloop.last %}, {% endunless %}{% endfor %})</code></a></h3>
{% if function.parameters %}
<table>
    <thead>
        <tr>
            <th>Parameter</th>
            <th>Type</th>
            <th>Description</th>
        </tr>
    </thead>
    <tbody>
    {% for param in function.parameters %}
        <tr>
            <td style="text-align: right;"><strong>{{ param.name }}</strong></td>
            <td><code>{{ param.type }}</code></td>
            <td>{{ param.desc | markdownify }}
                {% if param.type == "FUNCTION" %}
                {% include type-function.md params=param.parameters %}
                {% endif %}
                {% if param.type == "TABLE" %}
                {% include type-table.md fields=param.members %}
                {% endif %}
            </td>
        </tr>
        {% endfor %}
    </tbody>
</table>
{% endif %}
{% if function.returns %}
<h4>Returns</h4>
<code class="inline-code-block">{{ function.returns.type }}</code> {{ function.returns.desc | markdownify }}
{% endif %}

{{ function.desc | markdownify | replace: "[icon:attention]","<br><br>⚠️" }}

{% if function.examples %}
<h4>Examples</h4>
{% for example in function.examples %}
{{ example.desc | markdownify }}
{% endfor %}
{% endif %}
</div>

        {% endif %}
    {% endfor %}
{% endfor %}
