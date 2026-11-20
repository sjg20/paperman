# Configuration file for the Sphinx documentation builder.

project = 'Paperman'
copyright = '2009, Simon Glass'
author = 'Simon Glass'

extensions = []

exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    'collapse_navigation': False,
}

html_static_path = ['_static']
html_css_files = ['custom.css']

# The manual is the project's, so it carries the project's site, where the
# newest copy is, instead of a person's name
latex_documents = [
    ('manual', 'paperman.tex', 'Paperman User Manual', 'getpaperman.com',
     'manual'),
]

# Symbols which the default fonts do not have go in the preamble
latex_elements = {
    'extraclassoptions': 'openany,oneside',     # no blank pages on screen
    'preamble': r'''
\DeclareUnicodeCharacter{2318}{Cmd+}
\DeclareUnicodeCharacter{21C5}{\ensuremath{\uparrow\!\downarrow}}
''',
}
