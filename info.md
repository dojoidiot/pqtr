# PQTR Technical Overview

## System Architecture

PQTR consists of five core components that work together to provide a complete media platform solution.

### Core Components


#### 🖥️ **Host Infrastructure** (`host/`)
- **Technology**: Linux server hardening, SSH management
- **Purpose**: Secure server deployment and management of SaaS and Site projects.
- **Status**: Production ready with enterprise security
- **Start here**: `host/doc/apex.md`

#### 🚀 **SaaS Backend** (`saas/`)
- **Technology**: PostgreSQL + PostgREST
- **Purpose**: The Software as a Service platform for the PQTR data API.  Used by the site, pits, and user apps.
- **Status**: Production ready with multi-environment support
- **Start here**: `saas/doc/apex.md`


#### 🌐 **Site Management** (`site/`)
- **Technology**: Web application framework
- **Purpose**: The PQTR Website.
- **Status**: Basic structure complete, needs UI development
- **Start here**: `site/doc/apex.md`

#### 📱 **iOS Application** (`user/ios/`)
- **Technology**: React Native/Expo
- **Purpose**: Mobile PQTR interface for photographers to use with PITS and SaaS.
- **Status**: Complete app, ready for App Store submission
- **Start here**: `user/ios/doc/apex.md`

#### 📱 **Photo Image Transfer System** (`pits/`)
- **Technology**: WiFi hotspot, FTP server, web interface
- **Purpose**: Field photo transfer system for photographers
- **Status**: Core functionality complete, needs field testing
- **Start here**: `pits/doc/apex.md`

## Development Structure

- **Documentation**: Each project uses `doc/apex.md` convention
- **Scripts**: Unix-style organization (`bin/`, `etc/`, `src/`, `var/`, `www/`)
- **Configuration**: Centralized configs with environment-specific overrides
- **Testing**: Integrated testing and validation tools

## Current Status

- **Documentation**: Apex convention established across all projects
- **PITS System**: Complete with production-quality tools and documentation
- **Infrastructure**: Host and SaaS systems production-ready
- **Mobile**: iOS app complete and ready for deployment

## Next Steps

1. **Create apex.md files** for remaining project areas
2. **Implement documentation** for each project area
3. **Establish consistent structure** across all projects
4. **Maintain documentation standards** as projects evolve

---

*For business overview, see `boss.md`*  
*For development details, see each project's `doc/apex.md`*
