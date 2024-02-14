ifeq ($(strip $(MACCEL_ENABLE)), yes)
	SRC += $(USER_PATH)/features/maccel/maccel.c
	ifeq ($(strip $(VIA_ENABLE)), yes)
		SRC += $(USER_PATH)/features/maccel/maccel_via.c
	endif
	OPT_DEFS += -DMACCEL_ENABLE
endif
