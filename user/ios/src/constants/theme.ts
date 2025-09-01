export const theme = {
  colors: {
    background: '#F6F4F0',
    surface: '#FFFFFF',
    accent: '#1E1E1E',
    green: '#29694C',
    gray: '#8E8E93',
    lightGray: '#F2F2F7',
    border: '#EDEDED',
    text: {
      primary: '#1A1A1A',
      secondary: '#8E8E93',
      tertiary: '#C7C7CC',
    },
    status: {
      success: '#34C759',
      warning: '#FF9500',
      error: '#FF3B30',
    },
    // iOS system colors
    system: {
      blue: '#007AFF',
      green: '#34C759',
      indigo: '#5856D6',
      orange: '#FF9500',
      pink: '#FF2D92',
      purple: '#AF52DE',
      red: '#FF3B30',
      teal: '#5AC8FA',
      yellow: '#FFCC02',
    },
    // Semantic colors
    semantic: {
      primary: '#007AFF',
      secondary: '#5856D6',
      success: '#34C759',
      warning: '#FF9500',
      error: '#FF3B30',
      info: '#5AC8FA',
    }
  },
  spacing: {
    xs: 4,
    s: 8,
    m: 16,
    l: 24,
    xl: 32,
    xxl: 48,
  },
  fontSizes: {
    title: 20,
    body: 14,
    caption: 12,
    largeTitle: 28,
    headline: 18,
    // iOS typography scale
    ios: {
      largeTitle: 34,
      title1: 28,
      title2: 22,
      title3: 20,
      headline: 17,
      body: 17,
      callout: 16,
      subhead: 15,
      footnote: 13,
      caption1: 12,
      caption2: 11,
    }
  },
  borderRadius: {
    s: 8,
    m: 12,
    l: 16,
    xl: 20,
    xxl: 24,
  },
  shadows: {
    subtle: {
      shadowColor: 'rgba(0, 0, 0, 0.05)',
      shadowOffset: { width: 0, height: 1 },
      shadowOpacity: 1,
      shadowRadius: 2,
      elevation: 1,
    },
    medium: {
      shadowColor: 'rgba(0, 0, 0, 0.1)',
      shadowOffset: { width: 0, height: 2 },
      shadowOpacity: 1,
      shadowRadius: 4,
      elevation: 2,
    },
    large: {
      shadowColor: 'rgba(0, 0, 0, 0.15)',
      shadowOffset: { width: 0, height: 4 },
      shadowOpacity: 1,
      shadowRadius: 8,
      elevation: 4,
    }
  },
  // iOS-specific design tokens
  ios: {
    cornerRadius: {
      small: 8,
      medium: 12,
      large: 16,
      extraLarge: 20,
    },
    spacing: {
      tight: 4,
      compact: 8,
      comfortable: 16,
      loose: 24,
      extraLoose: 32,
    }
  }
}; 