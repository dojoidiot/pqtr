import React, { useState, useEffect } from 'react';
import { 
  View, 
  Text, 
  TextInput, 
  TouchableOpacity, 
  StyleSheet, 
  Modal,
  ScrollView,
  Alert
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';
import { availableTags } from '../mock/data';
import TagFilterChip from './TagFilterChip';

interface TagEditorModalProps {
  visible: boolean;
  onClose: () => void;
  onSave: (tags: string[]) => void;
  currentTags: string[];
}

const TagEditorModal: React.FC<TagEditorModalProps> = ({
  visible,
  onClose,
  onSave,
  currentTags
}) => {
  const [tags, setTags] = useState<string[]>(currentTags);
  const [newTag, setNewTag] = useState('');
  const [suggestions, setSuggestions] = useState<string[]>([]);

  useEffect(() => {
    setTags(currentTags);
  }, [currentTags]);

  const handleAddTag = (tag: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    const trimmedTag = tag.trim();
    
    if (trimmedTag && !tags.includes(trimmedTag)) {
      setTags([...tags, trimmedTag]);
      setNewTag('');
      setSuggestions([]);
    }
  };

  const handleRemoveTag = (tagToRemove: string) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setTags(tags.filter(tag => tag !== tagToRemove));
  };

  const handleInputChange = (text: string) => {
    setNewTag(text);
    
    if (text.trim()) {
      const filtered = availableTags.filter(tag => 
        tag.toLowerCase().includes(text.toLowerCase()) && 
        !tags.includes(tag)
      );
      setSuggestions(filtered.slice(0, 5));
    } else {
      setSuggestions([]);
    }
  };

  const handleSave = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onSave(tags);
    onClose();
  };

  const handleClose = () => {
    setTags(currentTags);
    setNewTag('');
    setSuggestions([]);
    onClose();
  };

  return (
    <Modal
      visible={visible}
      animationType="slide"
      presentationStyle="pageSheet"
      onRequestClose={handleClose}
    >
      <View style={styles.container}>
        {/* Header */}
        <View style={styles.header}>
          <TouchableOpacity onPress={handleClose} style={styles.closeButton}>
            <Ionicons name="close" size={24} color={theme.colors.text.primary} />
          </TouchableOpacity>
          <Text style={styles.title}>Edit Tags</Text>
          <TouchableOpacity onPress={handleSave} style={styles.saveButton}>
            <Text style={styles.saveButtonText}>Save</Text>
          </TouchableOpacity>
        </View>

        {/* Content */}
        <ScrollView style={styles.content} showsVerticalScrollIndicator={false}>
          {/* Current Tags */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Current Tags</Text>
            <View style={styles.tagsContainer}>
              {tags.map((tag, index) => (
                <View key={index} style={styles.tagWithRemove}>
                  <TagFilterChip
                    label={tag}
                    active={true}
                    onPress={() => {}} // Read-only
                  />
                  <TouchableOpacity
                    style={styles.removeButton}
                    onPress={() => handleRemoveTag(tag)}
                  >
                    <Ionicons name="close-circle" size={20} color={theme.colors.semantic.error} />
                  </TouchableOpacity>
                </View>
              ))}
              {tags.length === 0 && (
                <Text style={styles.noTagsText}>No tags added yet</Text>
              )}
            </View>
          </View>

          {/* Add New Tag */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Add New Tag</Text>
            <View style={styles.inputContainer}>
              <TextInput
                style={styles.input}
                value={newTag}
                onChangeText={handleInputChange}
                placeholder="Type a new tag..."
                placeholderTextColor={theme.colors.text.tertiary}
                onSubmitEditing={() => handleAddTag(newTag)}
                returnKeyType="done"
              />
              {newTag.trim() && (
                <TouchableOpacity
                  style={styles.addButton}
                  onPress={() => handleAddTag(newTag)}
                >
                  <Ionicons name="add" size={20} color={theme.colors.surface} />
                </TouchableOpacity>
              )}
            </View>
          </View>

          {/* Suggestions */}
          {suggestions.length > 0 && (
            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Suggestions</Text>
              <View style={styles.suggestionsContainer}>
                {suggestions.map((tag, index) => (
                  <TouchableOpacity
                    key={index}
                    style={styles.suggestionItem}
                    onPress={() => handleAddTag(tag)}
                  >
                    <Text style={styles.suggestionText}>{tag}</Text>
                    <Ionicons name="add" size={16} color={theme.colors.semantic.primary} />
                  </TouchableOpacity>
                ))}
              </View>
            </View>
          )}

          {/* Popular Tags */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Popular Tags</Text>
            <View style={styles.tagsContainer}>
              {availableTags.slice(0, 20).map((tag, index) => (
                <TouchableOpacity
                  key={index}
                  onPress={() => handleAddTag(tag)}
                  disabled={tags.includes(tag)}
                >
                  <TagFilterChip
                    label={tag}
                    active={tags.includes(tag)}
                    onPress={() => {}} // Handled by parent TouchableOpacity
                  />
                </TouchableOpacity>
              ))}
            </View>
          </View>
        </ScrollView>
      </View>
    </Modal>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: theme.colors.background,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
    backgroundColor: theme.colors.surface,
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  title: {
    fontSize: 18,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  saveButton: {
    backgroundColor: theme.colors.semantic.primary,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderRadius: theme.borderRadius.m,
  },
  saveButtonText: {
    color: theme.colors.surface,
    fontSize: 16,
    fontWeight: '600',
  },
  content: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  section: {
    marginTop: theme.spacing.l,
    paddingBottom: theme.spacing.m,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: '700',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.m,
  },
  tagsContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
  },
  tagWithRemove: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  removeButton: {
    padding: 2,
  },
  noTagsText: {
    fontSize: 14,
    color: theme.colors.text.tertiary,
    fontStyle: 'italic',
  },
  inputContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  input: {
    flex: 1,
    backgroundColor: theme.colors.surface,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    borderRadius: theme.borderRadius.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    fontSize: 16,
    color: theme.colors.text.primary,
  },
  addButton: {
    backgroundColor: theme.colors.semantic.primary,
    width: 44,
    height: 44,
    borderRadius: 22,
    justifyContent: 'center',
    alignItems: 'center',
  },
  suggestionsContainer: {
    gap: theme.spacing.s,
  },
  suggestionItem: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    backgroundColor: theme.colors.surface,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    borderRadius: theme.borderRadius.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  suggestionText: {
    fontSize: 14,
    color: theme.colors.text.primary,
  },
});

export default TagEditorModal;
