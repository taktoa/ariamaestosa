.PHONY : preinstall

CLEAN_DIRS += $(LOCAL_INSTALL_DIR) $(LOCAL_INSTALLDEV_DIR) $(LOCAL_INSTALLDOCSDEV_DIR)

preinstall : all lib tools tests examples docs
	@-$(MKDIR) -p $(LOCAL_INSTALL_BIN_DIR)
	@for i in $(LIB_TOOLS_EXE_FILES) $(LIB_GUI_EXE_FILES); do $(INSTALL) "$${i}" "$(LOCAL_INSTALL_BIN_DIR)"; done

.PHONY : install

install : preinstall
	@-$(MKDIR) -p $(INSTALL_BIN_DIR)
	@$(RSYNC) $(RSYNC_OPTIONS) $(LOCAL_INSTALL_BIN_DIR)/ $(INSTALL_BIN_DIR)

.PHONY : install-dev

preinstall-dev : preinstall 
	@-$(MKDIR) -p $(LOCAL_INSTALLDEV_INCLUDE_DIR) $(LOCAL_INSTALLDEV_LIB_DIR) $(LOCAL_INSTALLDEV_BIN_DIR)
	@for i in $(LIB_INCLUDE_DIR); do if [ -d "$${i}" ]; then $(RSYNC) $(RSYNC_OPTIONS) "$${i}"/* $(LOCAL_INSTALLDEV_INCLUDE_DIR); fi; done
	@$(RSYNC) $(RSYNC_OPTIONS) $(OUTPUT_LIB) $(LOCAL_INSTALLDEV_LIB_DIR) 
	@echo $(PROJECT_CONFIG_TOOL)
	@$(INSTALL) "$(CONFIG_TOOL_FILE)" "$(LOCAL_INSTALLDEV_BIN_DIR)"
	@for i in $(LIB_EXAMPLES_EXE_FILES) $(LIB_TESTS_EXE_FILES); do $(INSTALL) "$${i}" "$(LOCAL_INSTALLDEV_BIN_DIR)"; done

.PHONY : install-dev

install-dev : preinstall-dev
	@-$(MKDIR) -p $(INSTALLDEV_INCLUDE_DIR) $(INSTALLDEV_LIB_DIR) $(INSTALLDEV_BIN_DIR)
	@$(RSYNC) $(RSYNC_OPTIONS) $(LOCAL_INSTALLDEV_LIB_DIR)/ $(INSTALLDEV_LIB_DIR)
	@$(RSYNC) $(RSYNC_OPTIONS) $(LOCAL_INSTALLDEV_BIN_DIR)/ $(INSTALLDEV_BIN_DIR)
	@$(RSYNC) $(RSYNC_OPTIONS) $(LOCAL_INSTALLDEV_INCLUDE_DIR)/ $(INSTALLDEV_INCLUDE_DIR)

.PHONY : preinstall-docs-dev

preinstall-docs-dev : docs-dev 
	@-$(MKDIR) -p $(LOCAL_INSTALLDOCSDEV_DOCS_DIR)
	@$(RSYNC) $(RSYNC_OPTIONS) $(OUTPUT_DOCS_DIR)/ $(LOCAL_INSTALLDOCSDEV_DOCS_DIR)/api

.PHONY : install-docs-dev

install-docs-dev : preinstall-docs-dev
	@-$(MKDIR) -p $(INSTALLDOCSDEV_DOCS_DIR)/api
	@$(RSYNC) $(RSYNC_OPTIONS) $(LOCAL_INSTALLDOCSDEV_DIR)/$(TARGET_INSTALLDOCSDEV_DOCS_DIR)/ $(INSTALLDOCSDEV_DOCS_DIR)

.PHONY : preinstall-docs

preinstall-docs : docs 
	@-$(MKDIR) -p $(LOCAL_INSTALL_DOCS_DIR)
	@if [ -d $(LIB_DOCS_DIR) ]; then $(RSYNC) $(RSYNC_OPTIONS) $(LIB_DOCS_DIR)/ $(LOCAL_INSTALL_DOCS_DIR); fi

.PHONY : install-docs

install-docs : preinstall-docs 
	@-$(MKDIR) -p $(INSTALL_DOCS_DIR)
	@$(RSYNC) $(RSYNC_OPTIONS) $(LOCAL_INSTALL_DOCS_DIR)/ $(INSTALL_DOCS_DIR); fi
