import { useState, useEffect, useCallback } from 'react';
import { fakeDB, type User, type Project, type Photo, type Camera, type Preset, type Tag } from '../mock/database';

// ===== USER HOOKS =====
export function useCurrentUser() {
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchUser = async () => {
      try {
        setLoading(true);
        const userData = await fakeDB.getCurrentUser();
        setUser(userData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch user');
      } finally {
        setLoading(false);
      }
    };

    fetchUser();
  }, []);

  const updateUser = useCallback(async (updates: Partial<User>) => {
    if (!user) return;
    
    try {
      setLoading(true);
      const updatedUser = await fakeDB.updateUser(user.id, updates);
      setUser(updatedUser);
      setError(null);
      return updatedUser;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to update user');
      throw err;
    } finally {
      setLoading(false);
    }
  }, [user]);

  return { user, loading, error, updateUser };
}

// ===== PROJECT HOOKS =====
export function useProjects(userId: string) {
  const [projects, setProjects] = useState<Project[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchProjects = async () => {
      try {
        setLoading(true);
        console.log('Fetching projects for userId:', userId);
        const projectsData = await fakeDB.getProjects(userId);
        console.log('Fetched projects:', projectsData);
        setProjects(projectsData);
        setError(null);
      } catch (err) {
        console.error('Error fetching projects:', err);
        setError(err instanceof Error ? err.message : 'Failed to fetch projects');
      } finally {
        setLoading(false);
      }
    };

    if (userId) {
      fetchProjects();
    }
  }, [userId]);

  const createProject = useCallback(async (projectData: Omit<Project, 'id' | 'createdAt' | 'updatedAt'>) => {
    try {
      setLoading(true);
      const newProject = await fakeDB.createProject(projectData);
      setProjects(prev => [...prev, newProject]);
      setError(null);
      return newProject;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create project');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  const updateProject = useCallback(async (projectId: string, updates: Partial<Project>) => {
    try {
      setLoading(true);
      const updatedProject = await fakeDB.updateProject(projectId, updates);
      setProjects(prev => prev.map(p => p.id === projectId ? updatedProject : p));
      setError(null);
      return updatedProject;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to update project');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  const deleteProject = useCallback(async (projectId: string) => {
    try {
      setLoading(true);
      await fakeDB.deleteProject(projectId);
      setProjects(prev => prev.filter(p => p.id !== projectId));
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete project');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  return { 
    projects, 
    loading, 
    error, 
    createProject, 
    updateProject, 
    deleteProject,
    refetch: () => {
      setLoading(true);
      fakeDB.getProjects(userId).then(setProjects).finally(() => setLoading(false));
    }
  };
}

export function useProject(projectId: string) {
  const [project, setProject] = useState<Project | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchProject = async () => {
      try {
        setLoading(true);
        const projectData = await fakeDB.getProject(projectId);
        setProject(projectData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch project');
      } finally {
        setLoading(false);
      }
    };

    if (projectId) {
      fetchProject();
    }
  }, [projectId]);

  const updateProject = useCallback(async (updates: Partial<Project>) => {
    if (!project) return;
    
    try {
      setLoading(true);
      const updatedProject = await fakeDB.updateProject(project.id, updates);
      setProject(updatedProject);
      setError(null);
      return updatedProject;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to update project');
      throw err;
    } finally {
      setLoading(false);
    }
  }, [project]);

  return { project, loading, error, updateProject };
}

// ===== PHOTO HOOKS =====
export function usePhotos(projectId: string) {
  const [photos, setPhotos] = useState<Photo[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchPhotos = async () => {
      try {
        setLoading(true);
        const photosData = await fakeDB.getPhotos(projectId);
        setPhotos(photosData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch photos');
      } finally {
        setLoading(false);
      }
    };

    if (projectId) {
      fetchPhotos();
    }
  }, [projectId]);

  const createPhoto = useCallback(async (photoData: Omit<Photo, 'id' | 'createdAt' | 'updatedAt'>) => {
    try {
      setLoading(true);
      const newPhoto = await fakeDB.createPhoto(photoData);
      setPhotos(prev => [...prev, newPhoto]);
      setError(null);
      return newPhoto;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create photo');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  const updatePhoto = useCallback(async (photoId: string, updates: Partial<Photo>) => {
    try {
      setLoading(true);
      const updatedPhoto = await fakeDB.updatePhoto(photoId, updates);
      setPhotos(prev => prev.map(p => p.id === photoId ? updatedPhoto : p));
      setError(null);
      return updatedPhoto;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to update photo');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  const deletePhoto = useCallback(async (photoId: string) => {
    try {
      setLoading(true);
      await fakeDB.deletePhoto(photoId);
      setPhotos(prev => prev.filter(p => p.id !== photoId));
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete photo');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  return { 
    photos, 
    loading, 
    error, 
    createPhoto, 
    updatePhoto, 
    deletePhoto,
    refetch: () => {
      setLoading(true);
      fakeDB.getPhotos(projectId).then(setPhotos).finally(() => setLoading(false));
    }
  };
}

export function usePhoto(photoId: string) {
  const [photo, setPhoto] = useState<Photo | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchPhoto = async () => {
      try {
        setLoading(true);
        const photoData = await fakeDB.getPhoto(photoId);
        setPhoto(photoData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch photo');
      } finally {
        setLoading(false);
      }
    };

    if (photoId) {
      fetchPhoto();
    }
  }, [photoId]);

  const updatePhoto = useCallback(async (updates: Partial<Photo>) => {
    if (!photo) return;
    
    try {
      setLoading(true);
      const updatedPhoto = await fakeDB.updatePhoto(photo.id, updates);
      setPhoto(updatedPhoto);
      setError(null);
      return updatedPhoto;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to update photo');
      throw err;
    } finally {
      setLoading(false);
    }
  }, [photo]);

  return { photo, loading, error, updatePhoto };
}

// ===== CAMERA HOOKS =====
export function useCameras(userId: string) {
  const [cameras, setCameras] = useState<Camera[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchCameras = async () => {
      try {
        setLoading(true);
        const camerasData = await fakeDB.getCameras(userId);
        setCameras(camerasData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch cameras');
      } finally {
        setLoading(false);
      }
    };

    if (userId) {
      fetchCameras();
    }
  }, [userId]);

  const updateCameraStatus = useCallback(async (cameraId: string, status: Camera['status']) => {
    try {
      setLoading(true);
      const updatedCamera = await fakeDB.updateCameraStatus(cameraId, status);
      setCameras(prev => prev.map(c => c.id === cameraId ? updatedCamera : c));
      setError(null);
      return updatedCamera;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to update camera status');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  return { cameras, loading, error, updateCameraStatus };
}

// ===== PRESET HOOKS =====
export function usePresets(userId?: string, publicOnly: boolean = false) {
  const [presets, setPresets] = useState<Preset[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchPresets = async () => {
      try {
        setLoading(true);
        const presetsData = await fakeDB.getPresets(userId, publicOnly);
        setPresets(presetsData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch presets');
      } finally {
        setLoading(false);
      }
    };

    fetchPresets();
  }, [userId, publicOnly]);

  const createPreset = useCallback(async (presetData: Omit<Preset, 'id' | 'createdAt' | 'updatedAt'>) => {
    try {
      setLoading(true);
      const newPreset = await fakeDB.createPreset(presetData);
      setPresets(prev => [...prev, newPreset]);
      setError(null);
      return newPreset;
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create preset');
      throw err;
    } finally {
      setLoading(false);
    }
  }, []);

  return { presets, loading, error, createPreset };
}

export function usePreset(presetId: string) {
  const [preset, setPreset] = useState<Preset | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchPreset = async () => {
      try {
        setLoading(true);
        const presetData = await fakeDB.getPreset(presetId);
        setPreset(presetData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch preset');
      } finally {
        setLoading(false);
      }
    };

    if (presetId) {
      fetchPreset();
    }
  }, [presetId]);

  return { preset, loading, error };
}

// ===== TAG HOOKS =====
export function useTags(category?: string) {
  const [tags, setTags] = useState<Tag[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchTags = async () => {
      try {
        setLoading(true);
        const tagsData = await fakeDB.getTags(category);
        setTags(tagsData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch tags');
      } finally {
        setLoading(false);
      }
    };

    fetchTags();
  }, [category]);

  return { tags, loading, error };
}

export function useSearchTags() {
  const [searchResults, setSearchResults] = useState<Tag[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const searchTags = useCallback(async (query: string) => {
    if (!query.trim()) {
      setSearchResults([]);
      return;
    }

    try {
      setLoading(true);
      setError(null);
      const results = await fakeDB.searchTags(query);
      setSearchResults(results);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to search tags');
    } finally {
      setLoading(false);
    }
  }, []);

  return { searchResults, loading, error, searchTags };
}

// ===== SEARCH HOOKS =====
export function useSearchPhotos() {
  const [searchResults, setSearchResults] = useState<Photo[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const searchPhotos = useCallback(async (
    query: string, 
    filters?: {
      projectId?: string;
      tags?: string[];
      status?: Photo['status'];
      dateRange?: { start: string; end: string };
    }
  ) => {
    try {
      setLoading(true);
      setError(null);
      const results = await fakeDB.searchPhotos(query, filters);
      setSearchResults(results);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to search photos');
    } finally {
      setLoading(false);
    }
  }, []);

  return { searchResults, loading, error, searchPhotos };
}

// ===== STATISTICS HOOKS =====
export function useProjectStats(projectId: string) {
  const [stats, setStats] = useState<{
    totalPhotos: number;
    processedPhotos: number;
    exportedPhotos: number;
    publishedPhotos: number;
    totalTags: number;
    lastActivity: string;
  } | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchStats = async () => {
      try {
        setLoading(true);
        const statsData = await fakeDB.getProjectStats(projectId);
        setStats(statsData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch project stats');
      } finally {
        setLoading(false);
      }
    };

    if (projectId) {
      fetchStats();
    }
  }, [projectId]);

  return { stats, loading, error };
}

export function useUserStats(userId: string) {
  const [stats, setStats] = useState<{
    totalProjects: number;
    totalPhotos: number;
    totalPresets: number;
    totalCameras: number;
    storageUsed: number;
    subscriptionStatus: string;
  } | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchStats = async () => {
      try {
        setLoading(true);
        const statsData = await fakeDB.getUserStats(userId);
        setStats(statsData);
        setError(null);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to fetch user stats');
      } finally {
        setLoading(false);
      }
    };

    if (userId) {
      fetchStats();
    }
  }, [userId]);

  return { stats, loading, error };
}
