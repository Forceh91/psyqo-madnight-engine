// @ts-check
import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'Madnight Engine',
  tagline: 'A reusable psyqo-based engine layer for PS1 homebrew',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  url: 'https://forceh91.github.io',
  baseUrl: '/psyqo-madnight-engine/',

  organizationName: 'Forceh91',
  projectName: 'psyqo-madnight-engine',

  onBrokenLinks: 'throw',
  onBrokenAnchors: 'throw',
  onBrokenMarkdownLinks: 'throw',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: './sidebars.js',
          routeBasePath: '/',
          editUrl: 'https://github.com/Forceh91/psyqo-madnight-engine/tree/main/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      image: 'img/docusaurus-social-card.jpg',
      colorMode: {
        defaultMode: 'dark',
        respectPrefersColorScheme: true,
      },
      navbar: {
        title: 'Madnight Engine',
        logo: {
          alt: 'Madnight Engine Logo',
          src: 'img/logo.svg',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'docsSidebar',
            position: 'left',
            label: 'Docs',
          },
          {
            href: 'https://github.com/Forceh91/psyqo-madnight-engine',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              {label: 'Getting Started', to: '/getting-started/overview'},
              {label: 'API Reference', to: '/api/overview'},
            ],
          },
          {
            title: 'More',
            items: [
              {
                label: 'GitHub',
                href: 'https://github.com/Forceh91/psyqo-madnight-engine',
              },
              {
                label: 'PSX.Dev Discord',
                href: 'https://discord.gg/QByKPpH',
              },
            ],
          },
        ],
        copyright: `Madnight Engine docs · built with Docusaurus.`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
        additionalLanguages: ['cpp'],
      },
    }),
};

export default config;
