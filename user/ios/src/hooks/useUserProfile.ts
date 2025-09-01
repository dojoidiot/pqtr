import { useState, useCallback } from 'react';
import * as Haptics from 'expo-haptics';

export interface UserProfile {
  id: string;
  fullName: string;
  email: string;
  avatar: string;
  role: 'Photographer' | 'Editor' | 'Admin';
  subscriptionTier: 'Free' | 'Pro' | 'Enterprise';
  totalProjects: number;
  totalPhotos: number;
  syncsLast24h: number;
  storageUsed: number;
  storageTotal: number;
  preferences: {
    theme: 'light' | 'dark';
    notifications: boolean;
    defaultExportFormat: 'JPEG' | 'PNG' | 'RAW';
    autoApplyPresets: boolean;
  };
  collaborators: number;
  lastActive: string;
}

const mockUserProfile: UserProfile = {
  id: '1',
  fullName: 'Alex Chen',
  email: 'alex.chen@pqtr.com',
  avatar: 'https://images.unsplash.com/photo-1472099645785-5658abf4ff4e?w=150&h=150&fit=crop&crop=face',
  role: 'Photographer',
  subscriptionTier: 'Pro',
  totalProjects: 24,
  totalPhotos: 1247,
  syncsLast24h: 8,
  storageUsed: 2.4,
  storageTotal: 10,
  preferences: {
    theme: 'light',
    notifications: true,
    defaultExportFormat: 'JPEG',
    autoApplyPresets: false,
  },
  collaborators: 6,
  lastActive: '2024-01-15T14:30:00Z',
};

export const useUserProfile = () => {
  const [userProfile, setUserProfile] = useState<UserProfile>(mockUserProfile);
  const [isEditing, setIsEditing] = useState(false);
  const [showCollaborators, setShowCollaborators] = useState(false);
  const [showBilling, setShowBilling] = useState(false);

  const handleEditProfile = useCallback(() => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setIsEditing(true);
    // TODO: Open edit profile modal
  }, []);

  const handleThemeToggle = useCallback((value: boolean) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setUserProfile(prev => ({
      ...prev,
      preferences: {
        ...prev.preferences,
        theme: value ? 'dark' : 'light',
      },
    }));
  }, []);

  const handleNotificationsToggle = useCallback((value: boolean) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setUserProfile(prev => ({
      ...prev,
      preferences: {
        ...prev.preferences,
        notifications: value,
      },
    }));
  }, []);

  const handleAutoPresetsToggle = useCallback((value: boolean) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setUserProfile(prev => ({
      ...prev,
      preferences: {
        ...prev.preferences,
        autoApplyPresets: value,
      },
    }));
  }, []);

  const handleExportFormatChange = useCallback((format: 'JPEG' | 'PNG' | 'RAW') => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setUserProfile(prev => ({
      ...prev,
      preferences: {
        ...prev.preferences,
        defaultExportFormat: format,
      },
    }));
  }, []);

  const handleManageCollaborators = useCallback(() => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setShowCollaborators(true);
    // TODO: Open collaborators modal
  }, []);

  const handleManageBilling = useCallback(() => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    setShowBilling(true);
    // TODO: Open billing modal or external link
  }, []);

  const getStoragePercentage = useCallback(() => {
    return (userProfile.storageUsed / userProfile.storageTotal) * 100;
  }, [userProfile.storageUsed, userProfile.storageTotal]);

  const getStorageColor = useCallback(() => {
    const percentage = getStoragePercentage();
    if (percentage > 80) return '#FF3B30'; // error
    if (percentage > 60) return '#FF9500'; // warning
    return '#34C759'; // success
  }, [getStoragePercentage]);

  const formatLastActive = useCallback((timestamp: string) => {
    const date = new Date(timestamp);
    const now = new Date();
    const diffInHours = Math.floor((now.getTime() - date.getTime()) / (1000 * 60 * 60));
    
    if (diffInHours < 1) return 'Just now';
    if (diffInHours < 24) return `${diffInHours}h ago`;
    return date.toLocaleDateString();
  }, []);

  const updateProfile = useCallback((updates: Partial<UserProfile>) => {
    setUserProfile(prev => ({ ...prev, ...updates }));
  }, []);

  const resetPreferences = useCallback(() => {
    setUserProfile(prev => ({
      ...prev,
      preferences: {
        ...mockUserProfile.preferences,
      },
    }));
  }, []);

  return {
    userProfile,
    isEditing,
    showCollaborators,
    showBilling,
    handleEditProfile,
    handleThemeToggle,
    handleNotificationsToggle,
    handleAutoPresetsToggle,
    handleExportFormatChange,
    handleManageCollaborators,
    handleManageBilling,
    getStoragePercentage,
    getStorageColor,
    formatLastActive,
    updateProfile,
    resetPreferences,
    setIsEditing,
    setShowCollaborators,
    setShowBilling,
  };
};
