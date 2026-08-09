/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  docsSidebar: [
    'overview',
    {
      type: 'category',
      label: 'Getting Started',
      collapsed: false,
      items: [
        'getting-started/overview',
        'getting-started/toolchain',
        'getting-started/scaffolding-a-game',
        'getting-started/building-and-running',
        'getting-started/loading-assets',
        'getting-started/updating-the-engine',
      ],
    },
    {
      type: 'category',
      label: 'Asset Pipeline Guides',
      collapsed: false,
      items: [
        'guides/overview',
        'guides/meshbin',
        'guides/colbin',
        'guides/animbin',
        'guides/scenebin',
      ],
    },
    {
      type: 'category',
      label: 'API Reference',
      collapsed: false,
      items: [
        'api/overview',
        'api/core',
        'api/render',
        'api/mesh-and-animation',
        'api/physics-and-collision',
        'api/controller',
        'api/math',
        'api/sound',
        'api/textures',
        'api/ui',
        'api/helpers',
      ],
    },
  ],
};

export default sidebars;
