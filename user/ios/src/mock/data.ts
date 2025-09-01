// Legacy mock data - now using the comprehensive database system
export { fakeDB, type User, type Project, type Photo, type Camera, type Preset, type Tag } from './database';

// Legacy exports for backward compatibility
export const mockProjects = [
  {
    id: '1',
    name: 'Abu Dhabi 2024',
    description: 'Formula 1 Grand Prix weekend photography',
    photoCount: 1247,
    lastUpdated: '2 hours ago',
    coverImage: 'https://images.unsplash.com/photo-1536329583941-14287ec6fc4e?w=400',
    tags: ['F1', 'racing', 'sports'],
  },
  {
    id: '2',
    name: 'Desert Landscapes',
    description: 'Exploring the beauty of desert environments',
    photoCount: 892,
    lastUpdated: '1 day ago',
    coverImage: 'https://images.unsplash.com/photo-1501785888041-af3ef285b470?w=400',
    tags: ['desert', 'landscape', 'nature'],
  },
  {
    id: '3',
    name: 'Modern Architecture',
    description: 'Urban architecture and cityscapes',
    photoCount: 567,
    lastUpdated: '3 hours ago',
    coverImage: 'https://images.unsplash.com/photo-1553615738-d8fd7e89f3d6?w=400',
    tags: ['architecture', 'urban', 'city'],
  },
];

export const mockPhotos = [
  {
    id: '1',
    uri: 'https://images.unsplash.com/photo-1536329583941-14287ec6fc4e?w=1200',
    title: 'Abu Dhabi Grand Prix',
    tags: ['F1', 'racing', 'sports'],
    count: 10,
    published: true,
  },
  {
    id: '2',
    uri: 'https://images.unsplash.com/photo-1501785888041-af3ef285b470?w=1200',
    title: 'Desert Sunset',
    tags: ['desert', 'landscape', 'nature'],
    count: 8,
    published: false,
  },
  {
    id: '3',
    uri: 'https://images.unsplash.com/photo-1553615738-d8fd7e89f3d6?w=1200',
    title: 'Modern Building',
    tags: ['architecture', 'urban', 'city'],
    count: 6,
    published: true,
  },
];

export const mockPresets = [
  {
    id: '1',
    name: 'Ammyslife',
    category: 'Portrait',
    description: 'Warm, natural skin tones',
    downloads: 1247,
    rating: 4.8,
  },
  {
    id: '2',
    name: 'Black and White 1',
    category: 'Monochrome',
    description: 'Classic high-contrast B&W',
    downloads: 892,
    rating: 4.6,
  },
  {
    id: '3',
    name: 'Grain Boost',
    category: 'Film',
    description: 'Adds subtle film grain',
    downloads: 567,
    rating: 4.4,
  },
];

export const mockCameras = [
  {
    id: '1',
    name: "Peter's Sony A7iv",
    brand: 'Sony',
    model: 'A7 IV',
    status: 'connected',
    lastSync: 'Just now',
    photoCount: 1247,
  },
  {
    id: '2',
    name: 'Canon R5',
    brand: 'Canon',
    model: 'EOS R5',
    status: 'disconnected',
    lastSync: '2 days ago',
    photoCount: 892,
  },
  {
    id: '3',
    name: 'Nikon Z6 II',
    brand: 'Nikon',
    model: 'Z6 II',
    status: 'syncing',
    lastSync: '5 minutes ago',
    photoCount: 567,
  },
]; 