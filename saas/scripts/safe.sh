#!/bin/bash

# SaaS Project Environment Encryption Script
# Securely encrypt and decrypt environment files using GPG

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to show usage
show_usage() {
    echo "Usage: $0 {encrypt|decrypt|backup|restore|list|clean}"
    echo ""
    echo "Commands:"
    echo "  encrypt  - Encrypt all .env-* files to .env-*.gpg"
    echo "  decrypt  - Decrypt .env-*.gpg files to .env-*"
    echo "  backup   - Create encrypted backup of current environment"
    echo "  restore  - Restore environment from encrypted backup"
    echo "  list     - List available encrypted files"
    echo "  clean    - Remove decrypted .env-* files (keep encrypted)"
    echo ""
    echo "Examples:"
    echo "  $0 encrypt    # Encrypt all environment files"
    echo "  $0 decrypt    # Decrypt all environment files"
    echo "  $0 backup     # Backup current environment"
    echo "  $0 restore    # Restore from backup"
}

# Function to check if GPG is available
check_gpg() {
    if ! command -v gpg >/dev/null 2>&1; then
        echo -e "${RED}❌ GPG is not installed. Please install GPG first:${NC}"
        echo "   Ubuntu/Debian: sudo apt install gnupg"
        echo "   CentOS/RHEL: sudo yum install gnupg"
        echo "   macOS: brew install gnupg"
        exit 1
    fi
}

# Function to get password from user
get_password() {
    local prompt="$1"
    local confirm_prompt="$2"
    
    # Get password
    read -s -p "$prompt: " password
    echo ""
    
    # Confirm password
    read -s -p "$confirm_prompt: " password_confirm
    echo ""
    
    if [ "$password" != "$password_confirm" ]; then
        echo -e "${RED}❌ Passwords do not match${NC}"
        exit 1
    fi
    
    echo "$password"
}

# Function to encrypt environment files
encrypt_files() {
    echo -e "${BLUE}🔒 Encrypting environment files...${NC}"
    
    local password
    password=$(get_password "Enter encryption password" "Confirm encryption password")
    
    local encrypted_count=0
    
    # Find all .env-* files (excluding .gpg files)
    for env_file in .env-*; do
        if [[ -f "$env_file" && "$env_file" != *.gpg ]]; then
            echo "Encrypting $env_file..."
            
            # Encrypt file using GPG with symmetric encryption
            echo "$password" | gpg --batch --yes --passphrase-fd 0 \
                --symmetric --cipher-algo AES256 \
                --output "${env_file}.gpg" "$env_file"
            
            if [ $? -eq 0 ]; then
                echo -e "${GREEN}✅ Encrypted $env_file to ${env_file}.gpg${NC}"
                encrypted_count=$((encrypted_count + 1))
            else
                echo -e "${RED}❌ Failed to encrypt $env_file${NC}"
            fi
        fi
    done
    
    if [ $encrypted_count -gt 0 ]; then
        echo ""
        echo -e "${GREEN}🎉 Successfully encrypted $encrypted_count environment files${NC}"
        echo -e "${YELLOW}⚠️  Remember your password - you'll need it to decrypt!${NC}"
    else
        echo -e "${YELLOW}⚠️  No environment files found to encrypt${NC}"
    fi
}

# Function to decrypt environment files
decrypt_files() {
    echo -e "${BLUE}🔓 Decrypting environment files...${NC}"
    
    local password
    password=$(get_password "Enter decryption password" "Confirm decryption password")
    
    local decrypted_count=0
    
    # Find all .env-*.gpg files
    for gpg_file in .env-*.gpg; do
        if [[ -f "$gpg_file" ]]; then
            local env_file="${gpg_file%.gpg}"
            echo "Decrypting $gpg_file..."
            
            # Decrypt file using GPG
            echo "$password" | gpg --batch --yes --passphrase-fd 0 \
                --decrypt --output "$env_file" "$gpg_file"
            
            if [ $? -eq 0 ]; then
                echo -e "${GREEN}✅ Decrypted $gpg_file to $env_file${NC}"
                decrypted_count=$((decrypted_count + 1))
            else
                echo -e "${RED}❌ Failed to decrypt $gpg_file${NC}"
                echo "   Check your password and try again"
            fi
        fi
    done
    
    if [ $decrypted_count -gt 0 ]; then
        echo ""
        echo -e "${GREEN}🎉 Successfully decrypted $decrypted_count environment files${NC}"
    else
        echo -e "${YELLOW}⚠️  No encrypted files found to decrypt${NC}"
    fi
}

# Function to create encrypted backup
create_backup() {
    echo -e "${BLUE}💾 Creating encrypted backup...${NC}"
    
    # Check if .env exists
    if [ ! -f .env ]; then
        echo -e "${RED}❌ No .env file found to backup${NC}"
        echo "   Run setup or switch to an environment first"
        exit 1
    fi
    
    local password
    password=$(get_password "Enter backup encryption password" "Confirm backup encryption password")
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_file=".env.backup.${timestamp}.gpg"
    
    echo "Creating backup: $backup_file"
    
    # Encrypt current .env file
    echo "$password" | gpg --batch --yes --passphrase-fd 0 \
        --symmetric --cipher-algo AES256 \
        --output "$backup_file" .env
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Backup created: $backup_file${NC}"
        echo -e "${YELLOW}⚠️  Remember your backup password!${NC}"
    else
        echo -e "${RED}❌ Failed to create backup${NC}"
        exit 1
    fi
}

# Function to restore from encrypted backup
restore_backup() {
    echo -e "${BLUE}📥 Restoring from encrypted backup...${NC}"
    
    # List available backups
    local backups=()
    for backup in .env.backup.*.gpg; do
        if [[ -f "$backup" ]]; then
            backups+=("$backup")
        fi
    done
    
    if [ ${#backups[@]} -eq 0 ]; then
        echo -e "${YELLOW}⚠️  No backup files found${NC}"
        exit 1
    fi
    
    echo "Available backups:"
    for i in "${!backups[@]}"; do
        echo "  $((i+1)). ${backups[$i]}"
    done
    
    # Get user selection
    read -p "Select backup to restore (1-${#backups[@]}): " selection
    
    if ! [[ "$selection" =~ ^[0-9]+$ ]] || [ "$selection" -lt 1 ] || [ "$selection" -gt ${#backups[@]} ]; then
        echo -e "${RED}❌ Invalid selection${NC}"
        exit 1
    fi
    
    local selected_backup="${backups[$((selection-1))]}"
    echo "Selected: $selected_backup"
    
    local password
    password=$(get_password "Enter backup decryption password" "Confirm backup decryption password")
    
    # Backup current .env if it exists
    if [ -f .env ]; then
        local current_backup=".env.current.$(date +"%Y%m%d_%H%M%S")"
        cp .env "$current_backup"
        echo "Current .env backed up to: $current_backup"
    fi
    
    # Decrypt and restore
    echo "Restoring from $selected_backup..."
    echo "$password" | gpg --batch --yes --passphrase-fd 0 \
        --decrypt --output .env "$selected_backup"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Successfully restored from backup${NC}"
        echo "Current environment:"
        ./scripts/env-manager.sh show 2>/dev/null || echo "   (Environment manager not available)"
    else
        echo -e "${RED}❌ Failed to restore from backup${NC}"
        echo "   Check your password and try again"
        exit 1
    fi
}

# Function to list encrypted files
list_encrypted() {
    echo -e "${BLUE}📋 Encrypted Environment Files:${NC}"
    echo "=================================="
    
    local encrypted_count=0
    local backup_count=0
    
    # List .env-*.gpg files
    for gpg_file in .env-*.gpg; do
        if [[ -f "$gpg_file" ]]; then
            local env_file="${gpg_file%.gpg}"
            local size=$(du -h "$gpg_file" | cut -f1)
            local date=$(stat -c %y "$gpg_file" 2>/dev/null || stat -f %Sm "$gpg_file" 2>/dev/null || echo "Unknown")
            echo "🔒 $gpg_file -> $env_file ($size, $date)"
            encrypted_count=$((encrypted_count + 1))
        fi
    done
    
    echo ""
    echo -e "${BLUE}📋 Encrypted Backups:${NC}"
    echo "=========================="
    
    # List backup files
    for backup in .env.backup.*.gpg; do
        if [[ -f "$backup" ]]; then
            local size=$(du -h "$backup" | cut -f1)
            local date=$(stat -c %y "$backup" 2>/dev/null || stat -f %Sm "$backup" 2>/dev/null || echo "Unknown")
            echo "💾 $backup ($size, $date)"
            backup_count=$((backup_count + 1))
        fi
    done
    
    echo ""
    echo -e "${BLUE}📊 Summary:${NC}"
    echo "Encrypted files: $encrypted_count"
    echo "Backup files: $backup_count"
    
    if [ $encrypted_count -eq 0 ] && [ $backup_count -eq 0 ]; then
        echo -e "${YELLOW}⚠️  No encrypted files found${NC}"
    fi
}

# Function to clean decrypted files
clean_decrypted() {
    echo -e "${BLUE}🧹 Cleaning decrypted environment files...${NC}"
    
    local removed_count=0
    
    # Remove .env-* files (excluding .gpg files)
    for env_file in .env-*; do
        if [[ -f "$env_file" && "$env_file" != *.gpg ]]; then
            echo "Removing $env_file..."
            rm "$env_file"
            removed_count=$((removed_count + 1))
        fi
    done
    
    # Remove .env file if it exists
    if [ -f .env ]; then
        echo "Removing .env..."
        rm .env
        removed_count=$((removed_count + 1))
    fi
    
    if [ $removed_count -gt 0 ]; then
        echo -e "${GREEN}✅ Removed $removed_count decrypted files${NC}"
        echo -e "${YELLOW}⚠️  Encrypted .gpg files are preserved${NC}"
    else
        echo -e "${YELLOW}⚠️  No decrypted files found to remove${NC}"
    fi
}

# Main script logic
main() {
    # Check if GPG is available
    check_gpg
    
    case "$1" in
        encrypt)
            encrypt_files
            ;;
        decrypt)
            decrypt_files
            ;;
        backup)
            create_backup
            ;;
        restore)
            restore_backup
            ;;
        list)
            list_encrypted
            ;;
        clean)
            clean_decrypted
            ;;
        *)
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"
