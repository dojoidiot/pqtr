// Fake Database System
// Simulates real backend API calls and data persistence

// ===== DATA TYPES =====
export interface User {
  id: string;
  email: string;
  name: string;
  avatar?: string;
  subscription: 'free' | 'pro' | 'enterprise';
  createdAt: string;
  lastActive: string;
}

export interface Project {
  id: string;
  name: string;
  description?: string;
  userId: string;
  cameraId: string;
  presetId: string;
  exportFormats: string[];
  autoExport: boolean;
  status: 'draft' | 'active' | 'archived';
  photoCount: number;
  createdAt: string;
  updatedAt: string;
  tags: string[];
  coverPhotoId?: string;
  coverImage?: string; // Add this field for direct image access
  uploadProgress?: number; // 0..1 progress for uploads
  uploadStatus?: 'queued' | 'uploading' | 'paused' | 'error' | 'synced';
}

export interface Photo {
  id: string;
  projectId: string;
  uri: string;
  title: string;
  description?: string;
  tags: string[];
  metadata: {
    camera: string;
    lens: string;
    settings: {
      iso: number;
      shutterSpeed: string;
      aperture: string;
      focalLength: number;
    };
    location?: {
      latitude: number;
      longitude: number;
      name: string;
    };
    timestamp: string;
  };
  status: 'raw' | 'processed' | 'exported' | 'published';
  presetApplied?: string;
  exportFormats: string[];
  publishedAt?: string;
  createdAt: string;
  updatedAt: string;
  uploadProgress?: number; // 0..1 progress for uploads
  uploadStatus?: 'queued' | 'uploading' | 'paused' | 'error' | 'synced';
}

export interface Camera {
  id: string;
  userId: string;
  name: string;
  brand: string;
  model: string;
  type: 'dslr' | 'mirrorless' | 'compact' | 'phone' | 'rangefinder' | 'drone';
  status: 'connected' | 'disconnected' | 'syncing';
  lastSync?: string;
  photoCount: number;
  createdAt: string;
}

export interface Preset {
  id: string;
  name: string;
  category: string;
  description?: string;
  userId: string;
  isPublic: boolean;
  downloads: number;
  rating: number;
  tags: string[];
  previewImage?: string;
  settings: {
    exposure: number;
    contrast: number;
    saturation: number;
    temperature: number;
    tint: number;
    highlights: number;
    shadows: number;
    whites: number;
    blacks: number;
  };
  createdAt: string;
  updatedAt: string;
}

export interface Tag {
  id: string;
  name: string;
  category: string;
  usageCount: number;
  color?: string;
}

// ===== MOCK DATA =====
const USERS: User[] = [
  {
    id: 'user-1',
    email: 'peter@pqtr.com',
    name: 'Peter Smith',
    avatar: 'https://images.unsplash.com/photo-1472099645785-5658abf4ff4e?w=150',
    subscription: 'pro',
    createdAt: '2024-01-15T10:00:00Z',
    lastActive: new Date().toISOString(),
  },
  {
    id: 'user-2',
    email: 'sarah@pqtr.com',
    name: 'Sarah Johnson',
    avatar: 'https://images.unsplash.com/photo-1494790108755-2616b612b786?w=150',
    subscription: 'enterprise',
    createdAt: '2024-02-01T14:30:00Z',
    lastActive: new Date().toISOString(),
  },
];

const CAMERAS: Camera[] = [
  {
    id: 'camera-1',
    userId: 'user-1',
    name: "Peter's Sony A7iv",
    brand: 'Sony',
    model: 'A7 IV',
    type: 'mirrorless',
    status: 'connected',
    lastSync: new Date().toISOString(),
    photoCount: 1247,
    createdAt: '2024-01-20T09:00:00Z',
  },
  {
    id: 'camera-2',
    userId: 'user-1',
    name: 'Canon R5',
    brand: 'Canon',
    model: 'EOS R5',
    type: 'mirrorless',
    status: 'disconnected',
    photoCount: 892,
    createdAt: '2024-02-10T11:00:00Z',
  },
  {
    id: 'camera-3',
    userId: 'user-2',
    name: 'Nikon Z6 II',
    brand: 'Nikon',
    model: 'Z6 II',
    type: 'mirrorless',
    status: 'syncing',
    lastSync: new Date(Date.now() - 300000).toISOString(), // 5 minutes ago
    photoCount: 567,
    createdAt: '2024-01-25T16:00:00Z',
  },
  {
    id: 'camera-4',
    userId: 'user-1',
    name: 'Fujifilm X-T5',
    brand: 'Fujifilm',
    model: 'X-T5',
    type: 'mirrorless',
    status: 'connected',
    lastSync: new Date(Date.now() - 600000).toISOString(), // 10 minutes ago
    photoCount: 2341,
    createdAt: '2024-01-15T08:00:00Z',
  },
  {
    id: 'camera-5',
    userId: 'user-2',
    name: 'Leica M11',
    brand: 'Leica',
    model: 'M11',
    type: 'rangefinder',
    status: 'disconnected',
    photoCount: 156,
    createdAt: '2024-02-02T22:00:00Z',
  },
  {
    id: 'camera-6',
    userId: 'user-1',
    name: 'GoPro Hero 11',
    brand: 'GoPro',
    model: 'Hero 11 Black',
    type: 'compact',
    status: 'connected',
    lastSync: new Date(Date.now() - 1800000).toISOString(), // 30 minutes ago
    photoCount: 298,
    createdAt: '2024-02-01T09:00:00Z',
  },
  {
    id: 'camera-7',
    userId: 'user-2',
    name: 'Pentax K-3 III',
    brand: 'Pentax',
    model: 'K-3 III',
    type: 'dslr',
    status: 'syncing',
    lastSync: new Date(Date.now() - 900000).toISOString(), // 15 minutes ago
    photoCount: 678,
    createdAt: '2024-01-22T10:00:00Z',
  },
  {
    id: 'camera-8',
    userId: 'user-1',
    name: 'iPhone 15 Pro',
    brand: 'Apple',
    model: 'iPhone 15 Pro',
    type: 'phone',
    status: 'connected',
    lastSync: new Date(Date.now() - 120000).toISOString(), // 2 minutes ago
    photoCount: 1123,
    createdAt: '2024-01-18T16:00:00Z',
  },
  {
    id: 'camera-9',
    userId: 'user-2',
    name: 'Hasselblad X2D',
    brand: 'Hasselblad',
    model: 'X2D 100C',
    type: 'mirrorless',
    status: 'disconnected',
    photoCount: 89,
    createdAt: '2024-02-03T12:00:00Z',
  },
  {
    id: 'camera-10',
    userId: 'user-1',
    name: 'DJI Air 2S',
    brand: 'DJI',
    model: 'Air 2S',
    type: 'drone',
    status: 'connected',
    lastSync: new Date(Date.now() - 600000).toISOString(), // 10 minutes ago
    photoCount: 445,
    createdAt: '2024-01-10T13:00:00Z',
  }
];

const PRESETS: Preset[] = [
  {
    id: 'preset-1',
    name: 'Ammyslife',
    category: 'Portrait',
    description: 'Warm, natural skin tones with enhanced details',
    userId: 'user-1',
    isPublic: true,
    downloads: 1247,
    rating: 4.8,
    tags: ['portrait', 'warm', 'natural', 'skin'],
    previewImage: 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?w=300',
    settings: {
      exposure: 0.2,
      contrast: 15,
      saturation: -5,
      temperature: 5,
      tint: 2,
      highlights: -10,
      shadows: 15,
      whites: 5,
      blacks: -5,
    },
    createdAt: '2024-01-10T12:00:00Z',
    updatedAt: '2024-01-15T14:30:00Z',
  },
  {
    id: 'preset-2',
    name: 'Black and White 1',
    category: 'Monochrome',
    description: 'Classic high-contrast black and white',
    userId: 'user-1',
    isPublic: true,
    downloads: 892,
    rating: 4.6,
    tags: ['black-white', 'monochrome', 'contrast', 'classic'],
    previewImage: 'https://images.unsplash.com/photo-1503023345310-bd7c1de61c7d?w=300',
    settings: {
      exposure: 0,
      contrast: 25,
      saturation: -100,
      temperature: 0,
      tint: 0,
      highlights: 15,
      shadows: -10,
      whites: 10,
      blacks: -15,
    },
    createdAt: '2024-01-12T10:00:00Z',
    updatedAt: '2024-01-12T10:00:00Z',
  },
  {
    id: 'preset-3',
    name: 'Grain Boost',
    category: 'Film',
    description: 'Adds subtle film grain and enhances texture',
    userId: 'user-2',
    isPublic: true,
    downloads: 567,
    rating: 4.4,
    tags: ['film', 'grain', 'texture', 'vintage'],
    previewImage: 'https://images.unsplash.com/photo-1553615738-d8fd7e89f3d6?w=300',
    settings: {
      exposure: 0.1,
      contrast: 20,
      saturation: 10,
      temperature: 3,
      tint: -1,
      highlights: -5,
      shadows: 20,
      whites: 0,
      blacks: -10,
    },
    createdAt: '2024-01-18T15:00:00Z',
    updatedAt: '2024-01-20T09:15:00Z',
  },
  {
    id: 'preset-4',
    name: 'Moody Night',
    category: 'Atmosphere',
    description: 'Dark, moody tones perfect for night photography',
    userId: 'user-1',
    isPublic: false,
    downloads: 0,
    rating: 0,
    tags: ['moody', 'dark', 'night', 'atmosphere'],
    previewImage: 'https://images.unsplash.com/photo-1501785888041-af3ef285b470?w=300',
    settings: {
      exposure: -0.5,
      contrast: 30,
      saturation: -15,
      temperature: -5,
      tint: 3,
      highlights: -20,
      shadows: 25,
      whites: -10,
      blacks: 15,
    },
    createdAt: '2024-02-01T20:00:00Z',
    updatedAt: '2024-02-01T20:00:00Z',
  },
  {
    id: 'preset-5',
    name: 'Vintage Film',
    category: 'Film',
    description: 'Classic film look with faded colors',
    userId: 'user-2',
    isPublic: true,
    downloads: 1234,
    rating: 4.7,
    tags: ['vintage', 'film', 'faded', 'classic', 'retro'],
    previewImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=300',
    settings: {
      exposure: 0.3,
      contrast: 15,
      saturation: -20,
      temperature: 8,
      tint: -2,
      highlights: -15,
      shadows: 20,
      whites: -5,
      blacks: 10,
    },
    createdAt: '2024-01-15T14:00:00Z',
    updatedAt: '2024-01-18T16:30:00Z',
  },
  {
    id: 'preset-6',
    name: 'Bright & Airy',
    category: 'Portrait',
    description: 'Light, bright portrait style',
    userId: 'user-1',
    isPublic: true,
    downloads: 2156,
    rating: 4.9,
    tags: ['bright', 'airy', 'portrait', 'light', 'clean'],
    previewImage: 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?w=300',
    settings: {
      exposure: 0.5,
      contrast: -5,
      saturation: -10,
      temperature: 3,
      tint: 1,
      highlights: 15,
      shadows: 25,
      whites: 20,
      blacks: -5,
    },
    createdAt: '2024-01-08T10:00:00Z',
    updatedAt: '2024-01-12T11:45:00Z',
  },
  {
    id: 'preset-7',
    name: 'Urban Grit',
    category: 'Street',
    description: 'Gritty urban street photography style',
    userId: 'user-2',
    isPublic: true,
    downloads: 789,
    rating: 4.3,
    tags: ['urban', 'gritty', 'street', 'contrast', 'dramatic'],
    previewImage: 'https://images.unsplash.com/photo-1496442226666-8d4d0e62e6e9?w=300',
    settings: {
      exposure: 0.1,
      contrast: 35,
      saturation: -15,
      temperature: -3,
      tint: 2,
      highlights: -10,
      shadows: 30,
      whites: 5,
      blacks: 20,
    },
    createdAt: '2024-01-22T16:00:00Z',
    updatedAt: '2024-01-25T09:20:00Z',
  },
  {
    id: 'preset-8',
    name: 'Nature Vibrant',
    category: 'Nature',
    description: 'Enhanced natural colors for landscapes',
    userId: 'user-1',
    isPublic: true,
    downloads: 1678,
    rating: 4.6,
    tags: ['nature', 'vibrant', 'landscape', 'colorful', 'outdoor'],
    previewImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=300',
    settings: {
      exposure: 0.2,
      contrast: 20,
      saturation: 25,
      temperature: 5,
      tint: -1,
      highlights: -5,
      shadows: 15,
      whites: 10,
      blacks: -10,
    },
    createdAt: '2024-01-12T09:00:00Z',
    updatedAt: '2024-01-16T13:15:00Z',
  },
  {
    id: 'preset-9',
    name: 'Cinematic',
    category: 'Cinema',
    description: 'Movie-like cinematic look',
    userId: 'user-2',
    isPublic: true,
    downloads: 2341,
    rating: 4.8,
    tags: ['cinematic', 'movie', 'dramatic', 'wide', 'atmospheric'],
    previewImage: 'https://images.unsplash.com/photo-1499856871958-5b9627545d1a?w=300',
    settings: {
      exposure: -0.2,
      contrast: 25,
      saturation: -10,
      temperature: -2,
      tint: 1,
      highlights: -15,
      shadows: 20,
      whites: -5,
      blacks: 15,
    },
    createdAt: '2024-01-05T11:00:00Z',
    updatedAt: '2024-01-10T14:30:00Z',
  },
  {
    id: 'preset-10',
    name: 'Food Styling',
    category: 'Food',
    description: 'Perfect for food photography',
    userId: 'user-1',
    isPublic: true,
    downloads: 945,
    rating: 4.5,
    tags: ['food', 'styling', 'warm', 'appetizing', 'culinary'],
    previewImage: 'https://images.unsplash.com/photo-1565299624946-b28f40a0ca4b?w=300',
    settings: {
      exposure: 0.3,
      contrast: 15,
      saturation: 15,
      temperature: 8,
      tint: 2,
      highlights: -10,
      shadows: 20,
      whites: 15,
      blacks: -5,
    },
    createdAt: '2024-01-18T13:00:00Z',
    updatedAt: '2024-01-22T10:45:00Z',
  }
];

const PROJECTS: Project[] = [
  {
    id: 'project-1',
    name: 'Abu Dhabi 2024',
    description: 'Formula 1 Grand Prix weekend photography',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-2',
    exportFormats: ['9x16', '16x9'],
    autoExport: true,
    status: 'active',
    photoCount: 1247,
    createdAt: '2024-01-20T09:00:00Z',
    updatedAt: new Date().toISOString(),
    tags: ['F1', 'racing', 'sports', 'abudhabi'],
    coverPhotoId: 'photo-1',
    coverImage: 'https://images.unsplash.com/photo-1536329583941-14287ec6fc4e?w=1200',
    uploadProgress: 1,
    uploadStatus: 'synced',
  },
  {
    id: 'project-2',
    name: 'Desert Landscapes',
    description: 'Exploring the beauty of desert environments',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-3',
    exportFormats: ['16x9', '1x1'],
    autoExport: false,
    status: 'active',
    photoCount: 892,
    createdAt: '2024-01-25T14:00:00Z',
    updatedAt: new Date(Date.now() - 86400000).toISOString(), // 1 day ago
    tags: ['desert', 'landscape', 'nature', 'outdoor'],
    coverPhotoId: 'photo-2',
    coverImage: 'https://images.unsplash.com/photo-1501785888041-af3ef285b470?w=1200',
    uploadProgress: 0.75,
    uploadStatus: 'uploading',
  },
  {
    id: 'project-3',
    name: 'Modern Architecture',
    description: 'Urban architecture and cityscapes',
    userId: 'user-2',
    cameraId: 'camera-3',
    presetId: 'preset-1',
    exportFormats: ['16x9'],
    autoExport: true,
    status: 'active',
    photoCount: 567,
    createdAt: '2024-02-01T10:00:00Z',
    updatedAt: new Date(Date.now() - 3600000).toISOString(), // 1 hour ago
    tags: ['architecture', 'urban', 'city', 'modern'],
    coverPhotoId: 'photo-3',
    coverImage: 'https://images.unsplash.com/photo-1553615738-d8fd7e89f3d6?w=1200',
    uploadProgress: 0,
    uploadStatus: 'queued',
  },
  {
    id: 'project-4',
    name: 'Street Photography NYC',
    description: 'Capturing the energy of New York City streets',
    userId: 'user-1',
    cameraId: 'camera-2',
    presetId: 'preset-4',
    exportFormats: ['1x1', '4x5', '16x9'],
    autoExport: true,
    status: 'active',
    photoCount: 2156,
    createdAt: '2024-01-15T08:00:00Z',
    updatedAt: new Date(Date.now() - 1800000).toISOString(), // 30 minutes ago
    tags: ['street', 'nyc', 'urban', 'people', 'city-life'],
    coverPhotoId: 'photo-4',
    coverImage: 'https://images.unsplash.com/photo-1496442226666-8d4d0e62e6e9?w=1200',
    uploadProgress: 0.45,
    uploadStatus: 'uploading',
  },
  {
    id: 'project-5',
    name: 'Portrait Series',
    description: 'Professional headshots and creative portraits',
    userId: 'user-2',
    cameraId: 'camera-1',
    presetId: 'preset-1',
    exportFormats: ['1x1', '4x5'],
    autoExport: false,
    status: 'active',
    photoCount: 342,
    createdAt: '2024-01-28T14:00:00Z',
    updatedAt: new Date(Date.now() - 7200000).toISOString(), // 2 hours ago
    tags: ['portrait', 'headshot', 'professional', 'creative'],
    coverPhotoId: 'photo-5',
    coverImage: 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?w=1200',
    uploadProgress: 0,
    uploadStatus: 'error',
  },
  {
    id: 'project-6',
    name: 'Wedding Collection 2024',
    description: 'Beautiful moments from spring weddings',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-2',
    exportFormats: ['16x9', '4x5', '1x1'],
    autoExport: true,
    status: 'active',
    photoCount: 3891,
    createdAt: '2024-01-10T09:00:00Z',
    updatedAt: new Date(Date.now() - 300000).toISOString(), // 5 minutes ago
    tags: ['wedding', 'romance', 'celebration', 'spring', 'love'],
    coverPhotoId: 'photo-6',
    coverImage: 'https://images.unsplash.com/photo-1519741497674-611481863552?w=1200',
  },
  {
    id: 'project-7',
    name: 'Nature Macro',
    description: 'Close-up photography of flowers and insects',
    userId: 'user-2',
    cameraId: 'camera-3',
    presetId: 'preset-3',
    exportFormats: ['1x1', '4x3'],
    autoExport: false,
    status: 'active',
    photoCount: 178,
    createdAt: '2024-02-05T11:00:00Z',
    updatedAt: new Date(Date.now() - 86400000).toISOString(), // 1 day ago
    tags: ['macro', 'nature', 'flowers', 'insects', 'close-up'],
    coverPhotoId: 'photo-7',
    coverImage: 'https://images.unsplash.com/photo-1490750967868-88aa4486c946?w=1200',
  },
  {
    id: 'project-8',
    name: 'Car Photography',
    description: 'Automotive photography and car shows',
    userId: 'user-1',
    cameraId: 'camera-2',
    presetId: 'preset-4',
    exportFormats: ['16x9', '3x2'],
    autoExport: true,
    status: 'active',
    photoCount: 756,
    createdAt: '2024-01-22T16:00:00Z',
    updatedAt: new Date(Date.now() - 600000).toISOString(), // 10 minutes ago
    tags: ['cars', 'automotive', 'shows', 'racing', 'luxury'],
    coverPhotoId: 'photo-8',
    coverImage: 'https://images.unsplash.com/photo-1549317661-bd32c8ce0db2?w=1200',
  },
  {
    id: 'project-9',
    name: 'Food Photography',
    description: 'Culinary art and restaurant dishes',
    userId: 'user-2',
    cameraId: 'camera-1',
    presetId: 'preset-1',
    exportFormats: ['1x1', '4x5'],
    autoExport: false,
    status: 'active',
    photoCount: 423,
    createdAt: '2024-01-30T12:00:00Z',
    updatedAt: new Date(Date.now() - 3600000).toISOString(), // 1 hour ago
    tags: ['food', 'culinary', 'restaurant', 'cooking', 'art'],
    coverPhotoId: 'photo-9',
    coverImage: 'https://images.unsplash.com/photo-1565299624946-b28f40a0ca4b?w=1200',
  },
  {
    id: 'project-10',
    name: 'Travel Europe',
    description: 'Exploring cities across Europe',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-2',
    exportFormats: ['16x9', '1x1', '4x5'],
    autoExport: true,
    status: 'active',
    photoCount: 2987,
    createdAt: '2024-01-05T07:00:00Z',
    updatedAt: new Date(Date.now() - 120000).toISOString(), // 2 minutes ago
    tags: ['travel', 'europe', 'cities', 'culture', 'landmarks'],
    coverPhotoId: 'photo-10',
    coverImage: 'https://images.unsplash.com/photo-1499856871958-5b9627545d1a?w=1200',
  },
  {
    id: 'project-11',
    name: 'Concert Photography',
    description: 'Live music and performance shots',
    userId: 'user-2',
    cameraId: 'camera-2',
    presetId: 'preset-4',
    exportFormats: ['16x9', '3x2'],
    autoExport: true,
    status: 'active',
    photoCount: 1342,
    createdAt: '2024-01-18T20:00:00Z',
    updatedAt: new Date(Date.now() - 900000).toISOString(), // 15 minutes ago
    tags: ['concert', 'music', 'live', 'performance', 'artists'],
    coverPhotoId: 'photo-11',
    coverImage: 'https://images.unsplash.com/photo-1493225457124-a3eb161ffa5f?w=1200',
  },
  {
    id: 'project-12',
    name: 'Product Photography',
    description: 'Commercial product shots for e-commerce',
    userId: 'user-1',
    cameraId: 'camera-3',
    presetId: 'preset-1',
    exportFormats: ['1x1', '4x5'],
    autoExport: false,
    status: 'active',
    photoCount: 891,
    createdAt: '2024-01-12T13:00:00Z',
    updatedAt: new Date(Date.now() - 1800000).toISOString(), // 30 minutes ago
    tags: ['product', 'commercial', 'e-commerce', 'studio', 'lighting'],
    coverPhotoId: 'photo-12',
    coverImage: 'https://images.unsplash.com/photo-1441986300917-64674bd600d8?w=1200',
  },
  {
    id: 'project-13',
    name: 'Wildlife Safari',
    description: 'African wildlife photography expedition',
    userId: 'user-2',
    cameraId: 'camera-1',
    presetId: 'preset-3',
    exportFormats: ['16x9', '3x2'],
    autoExport: true,
    status: 'active',
    photoCount: 4567,
    createdAt: '2024-01-08T06:00:00Z',
    updatedAt: new Date(Date.now() - 60000).toISOString(), // 1 minute ago
    tags: ['wildlife', 'safari', 'africa', 'animals', 'nature'],
    coverPhotoId: 'photo-13',
    coverImage: 'https://images.unsplash.com/photo-1549366021-9f761d450615?w=1200',
  },
  {
    id: 'project-14',
    name: 'Fashion Editorial',
    description: 'High-end fashion photography and styling',
    userId: 'user-1',
    cameraId: 'camera-2',
    presetId: 'preset-2',
    exportFormats: ['16x9', '4x5', '1x1'],
    autoExport: true,
    status: 'active',
    photoCount: 2341,
    createdAt: '2024-01-25T15:00:00Z',
    updatedAt: new Date(Date.now() - 300000).toISOString(), // 5 minutes ago
    tags: ['fashion', 'editorial', 'styling', 'models', 'luxury'],
    coverPhotoId: 'photo-14',
    coverImage: 'https://images.unsplash.com/photo-1445205170230-053b83016050?w=1200',
  },
  {
    id: 'project-15',
    name: 'Astrophotography',
    description: 'Night sky and celestial photography',
    userId: 'user-2',
    cameraId: 'camera-3',
    presetId: 'preset-4',
    exportFormats: ['1x1', '16x9'],
    autoExport: false,
    status: 'active',
    photoCount: 156,
    createdAt: '2024-02-02T22:00:00Z',
    updatedAt: new Date(Date.now() - 86400000).toISOString(), // 1 day ago
    tags: ['astro', 'night', 'stars', 'celestial', 'long-exposure'],
    coverPhotoId: 'photo-15',
    coverImage: 'https://images.unsplash.com/photo-1534796636912-3b95b3ab5986?w=1200',
  },
  {
    id: 'project-16',
    name: 'Sports Action',
    description: 'Dynamic sports photography and motion',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-1',
    exportFormats: ['16x9', '3x2'],
    autoExport: true,
    status: 'active',
    photoCount: 3124,
    createdAt: '2024-01-15T10:00:00Z',
    updatedAt: new Date(Date.now() - 180000).toISOString(), // 3 minutes ago
    tags: ['sports', 'action', 'motion', 'athletes', 'competition'],
    coverPhotoId: 'photo-16',
    coverImage: 'https://images.unsplash.com/photo-1571019613454-1cb2f99b2d8b?w=1200',
  },
  {
    id: 'project-17',
    name: 'Documentary Series',
    description: 'Photojournalism and documentary work',
    userId: 'user-2',
    cameraId: 'camera-2',
    presetId: 'preset-3',
    exportFormats: ['16x9', '4x5'],
    autoExport: false,
    status: 'active',
    photoCount: 1876,
    createdAt: '2024-01-20T11:00:00Z',
    updatedAt: new Date(Date.now() - 7200000).toISOString(), // 2 hours ago
    tags: ['documentary', 'photojournalism', 'storytelling', 'reality'],
    coverPhotoId: 'photo-17',
    coverImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=1200',
  },
  {
    id: 'project-18',
    name: 'Vintage Cars',
    description: 'Classic and vintage automobile photography',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-2',
    exportFormats: ['16x9', '3x2', '1x1'],
    autoExport: true,
    status: 'active',
    photoCount: 654,
    createdAt: '2024-01-28T14:00:00Z',
    updatedAt: new Date(Date.now() - 600000).toISOString(), // 10 minutes ago
    tags: ['vintage', 'classic', 'cars', 'automotive', 'heritage'],
    coverPhotoId: 'photo-18',
    coverImage: 'https://images.unsplash.com/photo-1549317661-bd32c8ce0db2?w=1200',
  },
  {
    id: 'project-19',
    name: 'Underwater Photography',
    description: 'Marine life and underwater exploration',
    userId: 'user-2',
    cameraId: 'camera-3',
    presetId: 'preset-4',
    exportFormats: ['1x1', '16x9'],
    autoExport: false,
    status: 'active',
    photoCount: 298,
    createdAt: '2024-02-01T09:00:00Z',
    updatedAt: new Date(Date.now() - 86400000).toISOString(), // 1 day ago
    tags: ['underwater', 'marine', 'ocean', 'diving', 'sea-life'],
    coverPhotoId: 'photo-19',
    coverImage: 'https://images.unsplash.com/photo-1559827260-dc66d52bef19?w=1200',
  },
  {
    id: 'project-20',
    name: 'Urban Decay',
    description: 'Exploring abandoned and decaying urban spaces',
    userId: 'user-1',
    cameraId: 'camera-2',
    presetId: 'preset-3',
    exportFormats: ['16x9', '4x5'],
    autoExport: true,
    status: 'active',
    photoCount: 1123,
    createdAt: '2024-01-18T16:00:00Z',
    updatedAt: new Date(Date.now() - 1200000).toISOString(), // 20 minutes ago
    tags: ['urban-decay', 'abandoned', 'industrial', 'gritty', 'exploration'],
    coverPhotoId: 'photo-20',
    coverImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=1200',
  },
  {
    id: 'project-21',
    name: 'Minimalist Landscapes',
    description: 'Clean, simple landscape compositions',
    userId: 'user-2',
    cameraId: 'camera-1',
    presetId: 'preset-1',
    exportFormats: ['16x9', '1x1'],
    autoExport: false,
    status: 'draft',
    photoCount: 89,
    createdAt: '2024-02-03T12:00:00Z',
    updatedAt: new Date(Date.now() - 3600000).toISOString(), // 1 hour ago
    tags: ['minimalist', 'landscape', 'clean', 'simple', 'composition'],
    coverPhotoId: 'photo-21',
    coverImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=1200',
  },
  {
    id: 'project-22',
    name: 'Studio Portraits',
    description: 'Professional studio lighting and portraits',
    userId: 'user-1',
    cameraId: 'camera-3',
    presetId: 'preset-2',
    exportFormats: ['1x1', '4x5'],
    autoExport: true,
    status: 'archived',
    photoCount: 445,
    createdAt: '2024-01-10T13:00:00Z',
    updatedAt: new Date(Date.now() - 604800000).toISOString(), // 7 days ago
    tags: ['studio', 'portrait', 'lighting', 'professional', 'controlled'],
    coverPhotoId: 'photo-22',
    coverImage: 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?w=1200',
  },
  {
    id: 'project-23',
    name: 'Street Art',
    description: 'Graffiti and street art documentation',
    userId: 'user-2',
    cameraId: 'camera-2',
    presetId: 'preset-4',
    exportFormats: ['16x9', '1x1'],
    autoExport: false,
    status: 'active',
    photoCount: 678,
    createdAt: '2024-01-22T10:00:00Z',
    updatedAt: new Date(Date.now() - 1800000).toISOString(), // 30 minutes ago
    tags: ['street-art', 'graffiti', 'urban', 'art', 'culture'],
    coverPhotoId: 'photo-23',
    coverImage: 'https://images.unsplash.com/photo-1541961017774-22349e4a1262?w=1200',
  },
  {
    id: 'project-24',
    name: 'Mountain Photography',
    description: 'High altitude and mountain landscapes',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-3',
    exportFormats: ['16x9', '3x2'],
    autoExport: true,
    status: 'active',
    photoCount: 2341,
    createdAt: '2024-01-15T08:00:00Z',
    updatedAt: new Date(Date.now() - 900000).toISOString(), // 15 minutes ago
    tags: ['mountains', 'high-altitude', 'landscape', 'nature', 'adventure'],
    coverPhotoId: 'photo-24',
    coverImage: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=1200',
  },
  {
    id: 'project-25',
    name: 'Experimental Photography',
    description: 'Creative and experimental techniques',
    userId: 'user-2',
    cameraId: 'camera-3',
    presetId: 'preset-1',
    exportFormats: ['1x1', '16x9'],
    autoExport: false,
    status: 'draft',
    photoCount: 156,
    createdAt: '2024-02-04T15:00:00Z',
    updatedAt: new Date(Date.now() - 7200000).toISOString(), // 2 hours ago
    tags: ['experimental', 'creative', 'techniques', 'art', 'innovation'],
    coverPhotoId: 'photo-25',
    coverImage: 'https://images.unsplash.com/photo-1541961017774-22349e4a1262?w=1200',
  },
  {
    id: 'project-26',
    name: 'British Grand Prix 2025',
    description: 'Formula 1 British Grand Prix weekend photography - Silverstone Circuit',
    userId: 'user-1',
    cameraId: 'camera-1',
    presetId: 'preset-2',
    exportFormats: ['16x9', '9x16', '1x1'],
    autoExport: true,
    status: 'active',
    photoCount: 5,
    createdAt: '2025-07-05T08:00:00Z',
    updatedAt: new Date().toISOString(),
    tags: ['F1', 'racing', 'british-grand-prix', 'silverstone', 'formula1', 'motorsport'],
    coverPhotoId: 'photo-26',
    coverImage: 'https://images.unsplash.com/photo-1571019613454-1cb2f99b2d8b?w=1200',
    uploadProgress: 1,
    uploadStatus: 'synced',
  }
];

const PHOTOS: Photo[] = [
  {
    id: 'photo-1',
    projectId: 'project-1',
    uri: 'https://images.unsplash.com/photo-1536329583941-14287ec6fc4e?w=1200',
    title: 'Abu Dhabi Grand Prix',
    description: 'Main straight during qualifying',
    tags: ['F1', 'racing', 'abudhabi', 'qualifying'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 24-70mm f/2.8 GM',
      settings: {
        iso: 100,
        shutterSpeed: '1/1000',
        aperture: 'f/4',
        focalLength: 50,
      },
      location: {
        latitude: 24.4539,
        longitude: 54.3773,
        name: 'Yas Marina Circuit',
      },
      timestamp: '2024-01-20T15:30:00Z',
    },
    status: 'published',
    presetApplied: 'preset-2',
    exportFormats: ['9x16', '16x9'],
    publishedAt: '2024-01-21T10:00:00Z',
    createdAt: '2024-01-20T15:30:00Z',
    updatedAt: '2024-01-21T10:00:00Z',
    uploadProgress: 1,
    uploadStatus: 'synced',
  },
  {
    id: 'photo-2',
    projectId: 'project-2',
    uri: 'https://images.unsplash.com/photo-1501785888041-af3ef285b470?w=1200',
    title: 'Desert Sunset',
    description: 'Golden hour in the desert',
    tags: ['desert', 'sunset', 'golden-hour', 'landscape'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 16-35mm f/2.8 GM',
      settings: {
        iso: 64,
        shutterSpeed: '1/125',
        aperture: 'f/8',
        focalLength: 24,
      },
      location: {
        latitude: 24.4539,
        longitude: 54.3773,
        name: 'Abu Dhabi Desert',
      },
      timestamp: '2024-01-25T17:45:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-3',
    exportFormats: ['16x9', '1x1'],
    createdAt: '2024-01-25T17:45:00Z',
    updatedAt: '2024-01-26T09:00:00Z',
    uploadProgress: 0.6,
    uploadStatus: 'uploading',
  },
  {
    id: 'photo-3',
    projectId: 'project-3',
    uri: 'https://images.unsplash.com/photo-1553615738-d8fd7e89f3d6?w=1200',
    title: 'Modern Building',
    description: 'Contemporary architecture in the city',
    tags: ['architecture', 'modern', 'urban', 'building'],
    metadata: {
      camera: 'Nikon Z6 II',
      lens: 'Nikkor Z 24-70mm f/2.8 S',
      settings: {
        iso: 200,
        shutterSpeed: '1/250',
        aperture: 'f/5.6',
        focalLength: 35,
      },
      location: {
        latitude: 40.7128,
        longitude: -74.0060,
        name: 'New York City',
      },
      timestamp: '2024-02-01T14:20:00Z',
    },
    status: 'exported',
    presetApplied: 'preset-1',
    exportFormats: ['16x9'],
    createdAt: '2024-02-01T14:20:00Z',
    updatedAt: '2024-02-01T16:00:00Z',
  },
  {
    id: 'photo-4',
    projectId: 'project-4',
    uri: 'https://images.unsplash.com/photo-1496442226666-8d4d0e62e6e9?w=1200',
    title: 'NYC Street Scene',
    description: 'Busy Manhattan street corner',
    tags: ['street', 'nyc', 'urban', 'people', 'city-life'],
    metadata: {
      camera: 'Canon EOS R5',
      lens: 'RF 24-70mm f/2.8L IS USM',
      settings: {
        iso: 400,
        shutterSpeed: '1/60',
        aperture: 'f/2.8',
        focalLength: 35,
      },
      location: {
        latitude: 40.7589,
        longitude: -73.9851,
        name: 'Times Square, NYC',
      },
      timestamp: '2024-01-15T16:00:00Z',
    },
    status: 'published',
    presetApplied: 'preset-4',
    exportFormats: ['1x1', '4x5', '16x9'],
    publishedAt: '2024-01-16T09:00:00Z',
    createdAt: '2024-01-15T16:00:00Z',
    updatedAt: '2024-01-16T09:00:00Z',
  },
  {
    id: 'photo-5',
    projectId: 'project-5',
    uri: 'https://images.unsplash.com/photo-1507003211169-0a1dd7228f2d?w=1200',
    title: 'Professional Headshot',
    description: 'Corporate executive portrait',
    tags: ['portrait', 'headshot', 'professional', 'corporate'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 85mm f/1.4 GM',
      settings: {
        iso: 100,
        shutterSpeed: '1/200',
        aperture: 'f/2.8',
        focalLength: 85,
      },
      location: {
        latitude: 34.0522,
        longitude: -118.2437,
        name: 'Los Angeles Studio',
      },
      timestamp: '2024-01-28T14:30:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-1',
    exportFormats: ['1x1', '4x5'],
    createdAt: '2024-01-28T14:30:00Z',
    updatedAt: '2024-01-29T10:00:00Z',
  },
  {
    id: 'photo-6',
    projectId: 'project-6',
    uri: 'https://images.unsplash.com/photo-1519741497674-611481863552?w=1200',
    title: 'Wedding Ceremony',
    description: 'Beautiful outdoor wedding ceremony',
    tags: ['wedding', 'ceremony', 'romance', 'outdoor', 'celebration'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 24-70mm f/2.8 GM',
      settings: {
        iso: 200,
        shutterSpeed: '1/125',
        aperture: 'f/4',
        focalLength: 50,
      },
      location: {
        latitude: 34.0522,
        longitude: -118.2437,
        name: 'Beverly Hills Garden',
      },
      timestamp: '2024-01-10T17:00:00Z',
    },
    status: 'published',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '4x5', '1x1'],
    publishedAt: '2024-01-11T12:00:00Z',
    createdAt: '2024-01-10T17:00:00Z',
    updatedAt: '2024-01-11T12:00:00Z',
  },
  {
    id: 'photo-7',
    projectId: 'project-7',
    uri: 'https://images.unsplash.com/photo-1490750967868-88aa4486c946?w=1200',
    title: 'Flower Macro',
    description: 'Close-up of blooming rose',
    tags: ['macro', 'flower', 'rose', 'nature', 'close-up'],
    metadata: {
      camera: 'Nikon Z6 II',
      lens: 'Nikkor Z MC 105mm f/2.8 VR S',
      settings: {
        iso: 100,
        shutterSpeed: '1/200',
        aperture: 'f/5.6',
        focalLength: 105,
      },
      location: {
        latitude: 40.7128,
        longitude: -74.0060,
        name: 'Central Park, NYC',
      },
      timestamp: '2024-02-05T11:00:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-3',
    exportFormats: ['1x1', '4x3'],
    createdAt: '2024-02-05T11:00:00Z',
    updatedAt: '2024-02-06T08:00:00Z',
  },
  {
    id: 'photo-8',
    projectId: 'project-8',
    uri: 'https://images.unsplash.com/photo-1549317661-bd32c8ce0db2?w=1200',
    title: 'Luxury Sports Car',
    description: 'Ferrari on display at car show',
    tags: ['car', 'ferrari', 'luxury', 'sports', 'automotive'],
    metadata: {
      camera: 'Canon EOS R5',
      lens: 'RF 70-200mm f/2.8L IS USM',
      settings: {
        iso: 100,
        shutterSpeed: '1/250',
        aperture: 'f/4',
        focalLength: 135,
      },
      location: {
        latitude: 34.0522,
        longitude: -118.2437,
        name: 'LA Auto Show',
      },
      timestamp: '2024-01-22T16:00:00Z',
    },
    status: 'exported',
    presetApplied: 'preset-4',
    exportFormats: ['16x9', '3x2'],
    createdAt: '2024-01-22T16:00:00Z',
    updatedAt: '2024-01-23T09:00:00Z',
  },
  {
    id: 'photo-9',
    projectId: 'project-9',
    uri: 'https://images.unsplash.com/photo-1565299624946-b28f40a0ca4b?w=1200',
    title: 'Gourmet Pizza',
    description: 'Artisanal pizza with fresh ingredients',
    tags: ['food', 'pizza', 'gourmet', 'restaurant', 'culinary'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 90mm f/2.8 Macro G OSS',
      settings: {
        iso: 200,
        shutterSpeed: '1/100',
        aperture: 'f/4',
        focalLength: 90,
      },
      location: {
        latitude: 40.7589,
        longitude: -73.9851,
        name: 'NYC Restaurant',
      },
      timestamp: '2024-01-30T12:30:00Z',
    },
    status: 'raw',
    presetApplied: undefined,
    exportFormats: ['1x1', '4x5'],
    createdAt: '2024-01-30T12:30:00Z',
    updatedAt: '2024-01-30T12:30:00Z',
  },
  {
    id: 'photo-10',
    projectId: 'project-10',
    uri: 'https://images.unsplash.com/photo-1499856871958-5b9627545d1a?w=1200',
    title: 'Paris Eiffel Tower',
    description: 'Iconic landmark at sunset',
    tags: ['paris', 'eiffel-tower', 'landmark', 'sunset', 'travel'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 16-35mm f/2.8 GM',
      settings: {
        iso: 100,
        shutterSpeed: '1/60',
        aperture: 'f/8',
        focalLength: 24,
      },
      location: {
        latitude: 48.8584,
        longitude: 2.2945,
        name: 'Eiffel Tower, Paris',
      },
      timestamp: '2024-01-05T18:00:00Z',
    },
    status: 'published',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '1x1', '4x5'],
    publishedAt: '2024-01-06T10:00:00Z',
    createdAt: '2024-01-05T18:00:00Z',
    updatedAt: '2024-01-06T10:00:00Z',
  },
  {
    id: 'photo-11',
    projectId: 'project-11',
    uri: 'https://images.unsplash.com/photo-1493225457124-a3eb161ffa5f?w=1200',
    title: 'Rock Concert',
    description: 'Live performance with dramatic lighting',
    tags: ['concert', 'rock', 'live', 'performance', 'music'],
    metadata: {
      camera: 'Canon EOS R5',
      lens: 'RF 24-70mm f/2.8L IS USM',
      settings: {
        iso: 1600,
        shutterSpeed: '1/125',
        aperture: 'f/2.8',
        focalLength: 50,
      },
      location: {
        latitude: 34.0522,
        longitude: -118.2437,
        name: 'Hollywood Bowl',
      },
      timestamp: '2024-01-18T20:30:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-4',
    exportFormats: ['16x9', '3x2'],
    createdAt: '2024-01-18T20:30:00Z',
    updatedAt: '2024-01-19T14:00:00Z',
  },
  {
    id: 'photo-12',
    projectId: 'project-12',
    uri: 'https://images.unsplash.com/photo-1441986300917-64674bd600d8?w=1200',
    title: 'Product Shot',
    description: 'Professional product photography',
    tags: ['product', 'commercial', 'studio', 'lighting', 'e-commerce'],
    metadata: {
      camera: 'Nikon Z6 II',
      lens: 'Nikkor Z 24-70mm f/2.8 S',
      settings: {
        iso: 100,
        shutterSpeed: '1/200',
        aperture: 'f/8',
        focalLength: 50,
      },
      location: {
        latitude: 40.7128,
        longitude: -74.0060,
        name: 'NYC Studio',
      },
      timestamp: '2024-01-12T13:00:00Z',
    },
    status: 'exported',
    presetApplied: 'preset-1',
    exportFormats: ['1x1', '4x5'],
    createdAt: '2024-01-12T13:00:00Z',
    updatedAt: '2024-01-13T09:00:00Z',
  },
  {
    id: 'photo-13',
    projectId: 'project-13',
    uri: 'https://images.unsplash.com/photo-1549366021-9f761d450615?w=1200',
    title: 'Lion Portrait',
    description: 'Majestic lion in natural habitat',
    tags: ['wildlife', 'lion', 'safari', 'africa', 'animals'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 200-600mm f/5.6-6.3 G OSS',
      settings: {
        iso: 400,
        shutterSpeed: '1/500',
        aperture: 'f/6.3',
        focalLength: 400,
      },
      location: {
        latitude: -1.2921,
        longitude: 36.8219,
        name: 'Masai Mara, Kenya',
      },
      timestamp: '2024-01-08T07:00:00Z',
    },
    status: 'published',
    presetApplied: 'preset-3',
    exportFormats: ['16x9', '3x2'],
    publishedAt: '2024-01-09T12:00:00Z',
    createdAt: '2024-01-08T07:00:00Z',
    updatedAt: '2024-01-09T12:00:00Z',
  },
  {
    id: 'photo-14',
    projectId: 'project-14',
    uri: 'https://images.unsplash.com/photo-1445205170230-053b83016050?w=1200',
    title: 'Fashion Model',
    description: 'High-end fashion editorial shot',
    tags: ['fashion', 'model', 'editorial', 'styling', 'luxury'],
    metadata: {
      camera: 'Canon EOS R5',
      lens: 'RF 85mm f/1.2L USM DS',
      settings: {
        iso: 100,
        shutterSpeed: '1/200',
        aperture: 'f/2.8',
        focalLength: 85,
      },
      location: {
        latitude: 40.7589,
        longitude: -73.9851,
        name: 'NYC Fashion District',
      },
      timestamp: '2024-01-25T15:00:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '4x5', '1x1'],
    createdAt: '2024-01-25T15:00:00Z',
    updatedAt: '2024-01-26T11:00:00Z',
  },
  {
    id: 'photo-15',
    projectId: 'project-15',
    uri: 'https://images.unsplash.com/photo-1534796636912-3b95b3ab5986?w=1200',
    title: 'Night Sky',
    description: 'Starry night with Milky Way',
    tags: ['astro', 'night', 'stars', 'milky-way', 'long-exposure'],
    metadata: {
      camera: 'Nikon Z6 II',
      lens: 'Nikkor Z 14-24mm f/2.8 S',
      settings: {
        iso: 3200,
        shutterSpeed: '30s',
        aperture: 'f/2.8',
        focalLength: 14,
      },
      location: {
        latitude: 36.1699,
        longitude: -115.1398,
        name: 'Death Valley, CA',
      },
      timestamp: '2024-02-02T22:00:00Z',
    },
    status: 'raw',
    presetApplied: undefined,
    exportFormats: ['1x1', '16x9'],
    createdAt: '2024-02-02T22:00:00Z',
    updatedAt: '2024-02-02T22:00:00Z',
  },
  {
    id: 'photo-26',
    projectId: 'project-26',
    uri: 'https://images.unsplash.com/photo-1571019613454-1cb2f99b2d8b?w=1200',
    title: 'Silverstone Circuit - Main Straight',
    description: 'British Grand Prix qualifying session',
    tags: ['F1', 'racing', 'british-grand-prix', 'silverstone', 'qualifying'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 24-70mm f/2.8 GM',
      settings: {
        iso: 100,
        shutterSpeed: '1/1000',
        aperture: 'f/4',
        focalLength: 50,
      },
      location: {
        latitude: 52.0745,
        longitude: -1.0169,
        name: 'Silverstone Circuit',
      },
      timestamp: '2025-07-05T15:30:00Z',
    },
    status: 'published',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '9x16', '1x1'],
    publishedAt: '2025-07-05T16:00:00Z',
    createdAt: '2025-07-05T15:30:00Z',
    updatedAt: '2025-07-05T16:00:00Z',
    uploadProgress: 1,
    uploadStatus: 'synced',
  },
  {
    id: 'photo-27',
    projectId: 'project-26',
    uri: 'https://images.unsplash.com/photo-1549317661-bd32c8ce0db2?w=1200',
    title: 'Pit Lane Action',
    description: 'Teams preparing for the race',
    tags: ['F1', 'racing', 'british-grand-prix', 'silverstone', 'pit-lane'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 70-200mm f/2.8 GM',
      settings: {
        iso: 200,
        shutterSpeed: '1/500',
        aperture: 'f/2.8',
        focalLength: 135,
      },
      location: {
        latitude: 52.0745,
        longitude: -1.0169,
        name: 'Silverstone Circuit',
      },
      timestamp: '2025-07-05T16:15:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '9x16'],
    createdAt: '2025-07-05T16:15:00Z',
    updatedAt: '2025-07-05T17:00:00Z',
    uploadProgress: 1,
    uploadStatus: 'synced',
  },
  {
    id: 'photo-28',
    projectId: 'project-26',
    uri: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=1200',
    title: 'Corner Entry - Copse',
    description: 'High-speed corner entry at Copse',
    tags: ['F1', 'racing', 'british-grand-prix', 'silverstone', 'copse-corner'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 100-400mm f/4.5-5.6 GM',
      settings: {
        iso: 400,
        shutterSpeed: '1/800',
        aperture: 'f/5.6',
        focalLength: 300,
      },
      location: {
        latitude: 52.0745,
        longitude: -1.0169,
        name: 'Silverstone Circuit',
      },
      timestamp: '2025-07-05T16:45:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '9x16'],
    createdAt: '2025-07-05T16:45:00Z',
    updatedAt: '2025-07-05T17:30:00Z',
    uploadProgress: 1,
    uploadStatus: 'synced',
  },
  {
    id: 'photo-29',
    projectId: 'project-26',
    uri: 'https://images.unsplash.com/photo-1536329583941-14287ec6fc4e?w=1200',
    title: 'Start/Finish Line',
    description: 'Iconic Silverstone start/finish straight',
    tags: ['F1', 'racing', 'british-grand-prix', 'silverstone', 'start-finish'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 24-70mm f/2.8 GM',
      settings: {
        iso: 100,
        shutterSpeed: '1/1000',
        aperture: 'f/4',
        focalLength: 35,
      },
      location: {
        latitude: 52.0745,
        longitude: -1.0169,
        name: 'Silverstone Circuit',
      },
      timestamp: '2025-07-05T17:00:00Z',
    },
    status: 'published',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '9x16', '1x1'],
    publishedAt: '2025-07-05T17:30:00Z',
    createdAt: '2025-07-05T17:00:00Z',
    updatedAt: '2025-07-05T17:30:00Z',
    uploadProgress: 1,
    uploadStatus: 'synced',
  },
  {
    id: 'photo-30',
    projectId: 'project-26',
    uri: 'https://images.unsplash.com/photo-1501785888041-af3ef285b470?w=1200',
    title: 'Paddock Area',
    description: 'Team hospitality and support areas',
    tags: ['F1', 'racing', 'british-grand-prix', 'silverstone', 'paddock'],
    metadata: {
      camera: 'Sony A7 IV',
      lens: 'FE 16-35mm f/2.8 GM',
      settings: {
        iso: 200,
        shutterSpeed: '1/250',
        aperture: 'f/4',
        focalLength: 24,
      },
      location: {
        latitude: 52.0745,
        longitude: -1.0169,
        name: 'Silverstone Circuit',
      },
      timestamp: '2025-07-05T17:30:00Z',
    },
    status: 'processed',
    presetApplied: 'preset-2',
    exportFormats: ['16x9', '1x1'],
    createdAt: '2025-07-05T17:30:00Z',
    updatedAt: '2025-07-05T18:00:00Z',
    uploadProgress: 1,
    uploadStatus: 'synced',
  }
];

const TAGS: Tag[] = [
  { id: 'tag-1', name: 'F1', category: 'sports', usageCount: 1247, color: '#FF6B6B' },
  { id: 'tag-2', name: 'racing', category: 'sports', usageCount: 892, color: '#4ECDC4' },
  { id: 'tag-3', name: 'desert', category: 'nature', usageCount: 567, color: '#F7DC6F' },
  { id: 'tag-4', name: 'architecture', category: 'urban', usageCount: 456, color: '#BB8FCE' },
  { id: 'tag-5', name: 'portrait', category: 'people', usageCount: 789, color: '#85C1E9' },
  { id: 'tag-6', name: 'landscape', category: 'nature', usageCount: 654, color: '#82E0AA' },
  { id: 'tag-7', name: 'urban', category: 'city', usageCount: 432, color: '#F8C471' },
  { id: 'tag-8', name: 'modern', category: 'style', usageCount: 345, color: '#D7BDE2' },
  { id: 'tag-9', name: 'british-grand-prix', category: 'sports', usageCount: 5, color: '#E74C3C' },
  { id: 'tag-10', name: 'silverstone', category: 'location', usageCount: 5, color: '#3498DB' },
  { id: 'tag-11', name: 'pit-lane', category: 'sports', usageCount: 1, color: '#F39C12' },
  { id: 'tag-12', name: 'copse-corner', category: 'sports', usageCount: 1, color: '#9B59B6' },
  { id: 'tag-13', name: 'start-finish', category: 'sports', usageCount: 1, color: '#1ABC9C' },
  { id: 'tag-14', name: 'paddock', category: 'sports', usageCount: 1, color: '#E67E22' },
  { id: 'tag-15', name: 'qualifying', category: 'sports', usageCount: 1, color: '#34495E' }
];

// ===== DATABASE SERVICE =====
class FakeDatabaseService {
  private users = new Map(USERS.map(u => [u.id, u]));
  private projects = new Map(PROJECTS.map(p => [p.id, p]));
  private photos = new Map(PHOTOS.map(p => [p.id, p]));
  private cameras = new Map(CAMERAS.map(c => [c.id, c]));
  private presets = new Map(PRESETS.map(p => [p.id, p]));
  private tags = new Map(TAGS.map(t => [t.id, t]));

  // Simulate API delay
  private async delay(ms: number = 300) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  // ===== USER OPERATIONS =====
  async getCurrentUser(): Promise<User> {
    await this.delay();
    return USERS[0]; // Return Peter as current user
  }

  async updateUser(userId: string, updates: Partial<User>): Promise<User> {
    await this.delay();
    const user = this.users.get(userId);
    if (!user) throw new Error('User not found');
    
    const updatedUser = { ...user, ...updates, updatedAt: new Date().toISOString() };
    this.users.set(userId, updatedUser);
    return updatedUser;
  }

  // ===== PROJECT OPERATIONS =====
  async getProjects(userId: string): Promise<Project[]> {
    await this.delay();
    console.log('getProjects called with userId:', userId);
    console.log('All projects:', Array.from(this.projects.values()));
    const filteredProjects = Array.from(this.projects.values()).filter(p => p.userId === userId);
    console.log('Filtered projects for userId', userId, ':', filteredProjects);
    return filteredProjects;
  }

  async getProject(projectId: string): Promise<Project | null> {
    await this.delay();
    return this.projects.get(projectId) || null;
  }

  async createProject(projectData: Omit<Project, 'id' | 'createdAt' | 'updatedAt'>): Promise<Project> {
    await this.delay();
    const newProject: Project = {
      ...projectData,
      id: `project-${Date.now()}`,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.projects.set(newProject.id, newProject);
    return newProject;
  }

  async updateProject(projectId: string, updates: Partial<Project>): Promise<Project> {
    await this.delay();
    const project = this.projects.get(projectId);
    if (!project) throw new Error('Project not found');
    
    const updatedProject = { ...project, ...updates, updatedAt: new Date().toISOString() };
    this.projects.set(projectId, updatedProject);
    return updatedProject;
  }

  async deleteProject(projectId: string): Promise<void> {
    await this.delay();
    this.projects.delete(projectId);
    // Also delete associated photos
    Array.from(this.photos.values())
      .filter(p => p.projectId === projectId)
      .forEach(p => this.photos.delete(p.id));
  }

  // ===== PHOTO OPERATIONS =====
  async getPhotos(projectId: string): Promise<Photo[]> {
    await this.delay();
    return Array.from(this.photos.values()).filter(p => p.projectId === projectId);
  }

  async getPhoto(photoId: string): Promise<Photo | null> {
    await this.delay();
    return this.photos.get(photoId) || null;
  }

  async createPhoto(photoData: Omit<Photo, 'id' | 'createdAt' | 'updatedAt'>): Promise<Photo> {
    await this.delay();
    const newPhoto: Photo = {
      ...photoData,
      id: `photo-${Date.now()}`,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.photos.set(newPhoto.id, newPhoto);
    return newPhoto;
  }

  async updatePhoto(photoId: string, updates: Partial<Photo>): Promise<Photo> {
    await this.delay();
    const photo = this.photos.get(photoId);
    if (!photo) throw new Error('Photo not found');
    
    const updatedPhoto = { ...photo, ...updates, updatedAt: new Date().toISOString() };
    this.photos.set(photoId, updatedPhoto);
    return updatedPhoto;
  }

  async deletePhoto(photoId: string): Promise<void> {
    await this.delay();
    this.photos.delete(photoId);
  }

  // ===== CAMERA OPERATIONS =====
  async getCameras(userId: string): Promise<Camera[]> {
    await this.delay();
    return Array.from(this.cameras.values()).filter(c => c.userId === userId);
  }

  async getCamera(cameraId: string): Promise<Camera | null> {
    await this.delay();
    return this.cameras.get(cameraId) || null;
  }

  async updateCameraStatus(cameraId: string, status: Camera['status']): Promise<Camera> {
    await this.delay();
    const camera = this.cameras.get(cameraId);
    if (!camera) throw new Error('Camera not found');
    
    const updatedCamera = { 
      ...camera, 
      status, 
      lastSync: status === 'connected' ? new Date().toISOString() : camera.lastSync 
    };
    this.cameras.set(cameraId, updatedCamera);
    return updatedCamera;
  }

  // ===== PRESET OPERATIONS =====
  async getPresets(userId?: string, publicOnly: boolean = false): Promise<Preset[]> {
    await this.delay();
    let presets = Array.from(this.presets.values());
    
    if (publicOnly) {
      presets = presets.filter(p => p.isPublic);
    } else if (userId) {
      presets = presets.filter(p => p.userId === userId || p.isPublic);
    }
    
    return presets;
  }

  async getPreset(presetId: string): Promise<Preset | null> {
    await this.delay();
    return this.presets.get(presetId) || null;
  }

  async createPreset(presetData: Omit<Preset, 'id' | 'createdAt' | 'updatedAt'>): Promise<Preset> {
    await this.delay();
    const newPreset: Preset = {
      ...presetData,
      id: `preset-${Date.now()}`,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.presets.set(newPreset.id, newPreset);
    return newPreset;
  }

  // ===== TAG OPERATIONS =====
  async getTags(category?: string): Promise<Tag[]> {
    await this.delay();
    let tags = Array.from(this.tags.values());
    if (category) {
      tags = tags.filter(t => t.category === category);
    }
    return tags.sort((a, b) => b.usageCount - a.usageCount);
  }

  async searchTags(query: string): Promise<Tag[]> {
    await this.delay();
    const queryLower = query.toLowerCase();
    return Array.from(this.tags.values())
      .filter(t => t.name.toLowerCase().includes(queryLower))
      .sort((a, b) => b.usageCount - a.usageCount);
  }

  // ===== SEARCH OPERATIONS =====
  async searchPhotos(query: string, filters?: {
    projectId?: string;
    tags?: string[];
    status?: Photo['status'];
    dateRange?: { start: string; end: string };
  }): Promise<Photo[]> {
    await this.delay();
    let photos = Array.from(this.photos.values());
    
    // Text search
    if (query) {
      const queryLower = query.toLowerCase();
      photos = photos.filter(p => 
        p.title.toLowerCase().includes(queryLower) ||
        p.description?.toLowerCase().includes(queryLower) ||
        p.tags.some(tag => tag.toLowerCase().includes(queryLower))
      );
    }
    
    // Apply filters
    if (filters?.projectId) {
      photos = photos.filter(p => p.projectId === filters.projectId);
    }
    
    if (filters?.tags?.length) {
      photos = photos.filter(p => 
        filters.tags!.some(tag => p.tags.includes(tag))
      );
    }
    
    if (filters?.status) {
      photos = photos.filter(p => p.status === filters.status);
    }
    
    if (filters?.dateRange) {
      const start = new Date(filters.dateRange.start);
      const end = new Date(filters.dateRange.end);
      photos = photos.filter(p => {
        const photoDate = new Date(p.createdAt);
        return photoDate >= start && photoDate <= end;
      });
    }
    
    return photos.sort((a, b) => new Date(b.createdAt).getTime() - new Date(a.createdAt).getTime());
  }

  // ===== STATISTICS =====
  async getProjectStats(projectId: string): Promise<{
    totalPhotos: number;
    processedPhotos: number;
    exportedPhotos: number;
    publishedPhotos: number;
    totalTags: number;
    lastActivity: string;
  }> {
    await this.delay();
    const projectPhotos = Array.from(this.photos.values()).filter(p => p.projectId === projectId);
    
    return {
      totalPhotos: projectPhotos.length,
      processedPhotos: projectPhotos.filter(p => p.status !== 'raw').length,
      exportedPhotos: projectPhotos.filter(p => p.status === 'exported' || p.status === 'published').length,
      publishedPhotos: projectPhotos.filter(p => p.status === 'published').length,
      totalTags: new Set(projectPhotos.flatMap(p => p.tags)).size,
      lastActivity: projectPhotos.length > 0 
        ? projectPhotos.sort((a, b) => new Date(b.updatedAt).getTime() - new Date(a.updatedAt).getTime())[0].updatedAt
        : new Date().toISOString(),
    };
  }

  async getUserStats(userId: string): Promise<{
    totalProjects: number;
    totalPhotos: number;
    totalPresets: number;
    totalCameras: number;
    storageUsed: number; // in MB
    subscriptionStatus: string;
  }> {
    await this.delay();
    const userProjects = Array.from(this.projects.values()).filter(p => p.userId === userId);
    const userProjectIds = userProjects.map(p => p.id);
    const userPhotos = Array.from(this.photos.values()).filter(p => userProjectIds.includes(p.projectId));
    const userPresets = Array.from(this.presets.values()).filter(p => p.userId === userId);
    const userCameras = Array.from(this.cameras.values()).filter(c => c.userId === userId);
    
    return {
      totalProjects: userProjects.length,
      totalPhotos: userPhotos.length,
      totalPresets: userPresets.length,
      totalCameras: userCameras.length,
      storageUsed: userPhotos.length * 25, // Assume 25MB per photo
      subscriptionStatus: this.users.get(userId)?.subscription || 'free',
    };
  }
}

// Export singleton instance
export const fakeDB = new FakeDatabaseService();
