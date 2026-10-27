// SPDX-License-Identifier: GPL-2.0-only
/*
 * Samsung sound adaptation (SEC_SND_ADAPTATION): ASM sound-effect controls.
 *
 * Port of stock 4.4 sound/soc/msm/qdsp6v2/q6audio_adaptation.c, ASM part
 * only. The Maxim DSM, NXP TFADSP and voice (CVS/CVP) paths are not ported.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6asm-v2.h>
#include <dsp/q6adm-v2.h>
#include <dsp/sec_adaptation.h>
#include <dsp/q6voice.h>
#include <dsp/audio_cal_utils.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/rtac.h>
#include "../asoc/msm-pcm-routing-v2.h"

#define SEC_ADAPTATAION_AUDIO_PORT	3	/* ASM */

static struct mutex asm_lock;
#define TIMEOUT_MS	1000
#define SEC_ADAPTATAION_LOOPBACK_SRC_PORT	2	/* CVS */
#define SEC_ADAPTATAION_VOICE_SRC_PORT		2	/* CVP */

enum {
	LOOPBACK_DISABLE = 0,
	LOOPBACK_ENABLE,
	LOOPBACK_NODELAY,
	LOOPBACK_MAX,
};

struct cvs_ctl {
	void *apr;
	atomic_t state;
	wait_queue_head_t wait;
};

struct cvp_ctl {
	void *apr;
	atomic_t state;
	wait_queue_head_t wait;
};

static struct cvs_ctl this_cvs;
static struct cvp_ctl this_cvp;
static struct common_data *common;

struct afe_port {
	unsigned int voice_tracking_id;
	unsigned int amp_rx_id;
	unsigned int amp_tx_id;
	unsigned int rx_topology;
	unsigned int tx_topology;
};
static struct afe_port afe_port;

static int loopback_mode;
static int loopback_prev_mode;


static int q6asm_set_sound_alive(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_sa cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;
	int i = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_SA;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_SA_PARAMS;
	param_info.param_size = sizeof(cmd);

	cmd.OutDevice = param[0];
	cmd.Preset = param[1];
	for (i = 0; i < 9; i++)
		cmd.EqLev[i] = param[i + 2];
	cmd.m3Dlevel = param[11];
	cmd.BElevel = param[12];
	cmd.CHlevel = param[13];
	cmd.CHRoomSize = param[14];
	cmd.Clalevel = param[15];
	cmd.volume = param[16];
	cmd.Sqrow = param[17];
	cmd.Sqcol = param[18];
	cmd.TabInfo = param[19];
	cmd.NewUI = param[20];
	cmd.m3DPositionOn = param[21];
	cmd.m3DPositionAngle[0] = param[22];
	cmd.m3DPositionAngle[1] = param[23];
	cmd.m3DPositionGain[0] = param[24];
	cmd.m3DPositionGain[1] = param[25];
	cmd.AHDRonoff = param[26];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_play_speed(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_vsp cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_SA_VSP;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_SA_VSP_PARAMS;
	param_info.param_size = sizeof(cmd);

	cmd.speed_int = param[0];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_adaptation_sound(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_adaptation_sound cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;
	int i = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_ADAPTATION_SOUND;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_ADAPTATION_SOUND_PARAMS;
	param_info.param_size = sizeof(cmd);

	cmd.enable = param[0];
	for (i = 0; i < 12; i++)
		cmd.gain[i / 6][i % 6] = param[i + 1];
	cmd.device = param[13];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_sound_balance(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_lrsm cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_LRSM;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_LRSM_PARAMS;
	param_info.param_size = sizeof(cmd);

	cmd.sm = param[0];
	cmd.lr = param[1];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_myspace(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_msp cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_SA_MSP;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_MODULE_ID_PP_SA_MSP_PARAM;
	param_info.param_size = sizeof(cmd);

	cmd.msp_int = param[0];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_sound_boost(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_sb cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_SB;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_SB_PARAM;
	param_info.param_size = sizeof(cmd);

	cmd.sb_enable = param[0];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_upscaler(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_upscaler cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_SA_UPSCALER_COLOR;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_SA_UPSCALER_COLOR_PARAMS;
	param_info.param_size = sizeof(cmd);

	cmd.upscaler_enable = param[0];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

static int q6asm_set_sb_rotation(struct audio_client *ac, long *param)
{
	struct asm_stream_cmd_set_pp_params_sb_rotation cmd;
	struct param_hdr_v3 param_info;
	int rc = 0;

	if (ac == NULL) {
		pr_err("%s: audio client is null\n", __func__);
		return -EINVAL;
	}

	memset(&param_info, 0, sizeof(param_info));
	memset(&cmd, 0, sizeof(cmd));
	param_info.module_id = ASM_MODULE_ID_PP_SB;
	param_info.instance_id = INSTANCE_ID_0;
	param_info.param_id = ASM_PARAM_ID_PP_SB_ROTATION_PARAM;
	param_info.param_size = sizeof(cmd);

	cmd.sb_rotation = (unsigned int)param[0];

	rc = q6asm_pack_and_set_pp_param_in_band(ac, param_info,
						 (u8 *)&cmd);
	if (rc)
		pr_err("%s: set-params send failed paramid[0x%x] rc %d\n",
			__func__, param_info.param_id, rc);
	return rc;
}

/*
 * Each put handler resolves the ASM client of the audio front end on
 * SEC_SND_ADAPTATAION_AUDIO_PORT and forwards the user values. The values
 * are the SOC_SINGLE_MULTI_EXT array, whose length is fixed per control.
 */
static int sec_audio_sound_alive_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_play_speed_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_adaptation_sound_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sound_balance_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_voice_tracking_info_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{

	int rc = 0;
	int be_idx = 0;
	char *param_value;
	int *update_param_value;
	uint32_t param_size = (RMS_PAYLOAD_LEN + 1) * sizeof(uint32_t);
	struct msm_pcm_routing_bdai_data msm_bedai;
	struct param_hdr_v3 param_hdr;

	param_value = kzalloc(param_size, GFP_KERNEL);
	if (!param_value) {
		pr_err("%s, param memory alloc failed\n", __func__);
		return -ENOMEM;
	}

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);
		if (msm_bedai.port_id == afe_port.voice_tracking_id)
			break;
	}
	if ((be_idx < MSM_BACKEND_DAI_MAX) && msm_bedai.active) {
		memset(&param_hdr, 0, sizeof(param_hdr));
		param_hdr.module_id = ADM_MODULE_ID_PP_SS_REC;
		param_hdr.instance_id = 0x8000;
		param_hdr.param_id = ADM_PARAM_ID_PP_SS_REC_GETPARAMS;
		param_hdr.param_size = param_size;
		rc = adm_get_pp_params(afe_port.voice_tracking_id, 0,
				ADM_CLIENT_ID_DEFAULT, NULL, &param_hdr, param_value);
		if (rc) {
			pr_err("%s: get parameters failed:%d\n", __func__, rc);
			kfree(param_value);
			return -EINVAL;
		}
		update_param_value = (int *)param_value;
		ucontrol->value.integer.value[0] = update_param_value[0];

		pr_debug("%s: FROM DSP value[0] 0x%x\n",
			  __func__, update_param_value[0]);
	}
	kfree(param_value);

	return 0;
}

static int sec_audio_myspace_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sound_boost_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_upscaler_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sb_rotation_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_sound_alive_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sound_alive(ac,
		(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);

	return ret;
}

static int sec_audio_play_speed_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_play_speed(ac,
		(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);

	return ret;
}

static int sec_audio_adaptation_sound_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_adaptation_sound(ac,
		(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sound_balance_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sound_balance(ac,
		(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_voice_tracking_info_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	return ret;
}

static int sec_audio_myspace_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_myspace(ac, (long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sound_boost_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sound_boost(ac, (long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);

	return ret;
}

static int sec_audio_upscaler_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_upscaler(ac, (long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);

	return ret;
}

static int sec_audio_sb_rotation_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_pcm_routing_fdai_data fe_dai_map;
	struct audio_client *ac;

	mutex_lock(&asm_lock);
	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	ac = q6asm_get_audio_client(fe_dai_map.strm_id);
	ret = q6asm_set_sb_rotation(ac, (long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);

	return ret;
}

/****************************************************************************/
/*//////////////////////////// VOICE SOLUTION //////////////////////////////*/
/****************************************************************************/
/*//////////////////////////// COMMON COMMAND //////////////////////////////*/
/* This function must be sync up from voice_get_session_by_idx() of q6voice.c */
static struct voice_data *voice_get_session_by_idx(int idx)
{
	return ((idx < 0 || idx >= MAX_VOC_SESSIONS) ?
				NULL : &common->voice[idx]);
}

/* This function must be sync up */
/* from voice_itr_get_next_session() */
/* of q6voice.c */
static bool voice_itr_get_next_session(struct voice_session_itr *itr,
					struct voice_data **voice)
{
	bool ret = false;

	if (itr == NULL)
		return false;
	pr_debug("%s : cur idx = %d session idx = %d\n",
			 __func__, itr->cur_idx, itr->session_idx);

	if (itr->cur_idx <= itr->session_idx) {
		ret = true;
		*voice = voice_get_session_by_idx(itr->cur_idx);
		itr->cur_idx++;
	} else {
		*voice = NULL;
	}

	return ret;
}

/* This function must be sync up from voice_itr_init() of q6voice.c */
static void voice_itr_init(struct voice_session_itr *itr,
			   u32 session_id)
{
	if (itr == NULL)
		return;
	itr->session_idx = voice_get_idx_for_session(session_id);
	if (session_id == ALL_SESSION_VSID)
		itr->cur_idx = 0;
	else
		itr->cur_idx = itr->session_idx;
}

/* This function must be sync up from is_voc_state_active() of q6voice.c */
static bool is_voc_state_active(int voc_state)
{
	if ((voc_state == VOC_RUN) ||
		(voc_state == VOC_CHANGE) ||
		(voc_state == VOC_STANDBY))
		return true;

	return false;
}

/* This function must be sync up from voc_set_error_state() of q6voice.c */
static void voc_set_error_state(uint16_t reset_proc)
{
	struct voice_data *v = NULL;
	int i;

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		v = &common->voice[i];
		if (v != NULL) {
			v->voc_state = VOC_ERROR;
			v->rec_info.recording = 0;
		}
	}
}

/* This function must be sync up from */
/* is_source_tracking_shared_memomry_allocated() */
/* of q6voice.c */
static int is_source_tracking_shared_memomry_allocated(void)
{
	bool ret;

	pr_debug("%s: Enter\n", __func__);

	if (common->source_tracking_sh_mem.sh_mem_block.dma_buf != NULL)
		ret = true;
	else
		ret = false;

	pr_debug("%s: Exit\n", __func__);

	return ret;
}

/*//////////////////////////// CVS COMMAND //////////////////////////////*/
/* This function must be sync up from voice_get_cvs_handle() of q6voice.c */
static u16 voice_get_cvs_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: cvs_handle %d\n", __func__, v->cvs_handle);

	return v->cvs_handle;
}

static int32_t q6audio_adaptation_cvs_callback(struct apr_client_data *data,
	void *priv)
{
	int i;
	uint32_t *ptr = NULL;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_cvs.apr);

		if (this_cvs.apr) {
			apr_reset(this_cvs.apr);
			atomic_set(&this_cvs.state, 0);
			this_cvs.apr = NULL;
		}

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			common->voice[i].cvs_handle = 0;

		cal_utils_clear_cal_block_q6maps(MAX_VOICE_CAL_TYPES,
				common->cal_data);

		/* Free the ION memory and clear handles for Source Tracking */
		if (is_source_tracking_shared_memomry_allocated()) {
			msm_audio_ion_free(common->source_tracking_sh_mem.sh_mem_block.dma_buf);
			common->source_tracking_sh_mem.mem_handle = 0;
			common->source_tracking_sh_mem.sh_mem_block.dma_buf = NULL;
		}

		voc_set_error_state(data->reset_proc);
		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size >= 2 * sizeof(uint32_t)) {
			ptr = data->payload;
			pr_debug("%x %x\n", ptr[0], ptr[1]);
			if (ptr[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, ptr[0], ptr[1]);
			}

			switch (ptr[0]) {
			case VSS_ICOMMON_CMD_SET_PARAM_V2:
				pr_info("%s: VSS_ICOMMON_CMD_SET_PARAM_V2\n",
					__func__);
				atomic_set(&this_cvs.state, 0);
				wake_up(&this_cvs.wait);
				break;
			default:
				pr_err("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	}
	return 0;
}

static int send_packet_loopback_cmd(struct voice_data *v, int mode)
{
	struct cvs_set_loopback_enable_cmd cvs_set_loopback_cmd;
	int ret = 0;
	u16 cvs_handle;

	if (this_cvs.apr == NULL) {
		this_cvs.apr = apr_register("ADSP", "CVS",
					q6audio_adaptation_cvs_callback,
					SEC_ADAPTATAION_LOOPBACK_SRC_PORT,
					&this_cvs);
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* fill in the header */
	cvs_set_loopback_cmd.hdr.hdr_field =
	APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_set_loopback_cmd.hdr.pkt_size =
		APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_set_loopback_cmd) - APR_HDR_SIZE);
	cvs_set_loopback_cmd.hdr.src_port =
		SEC_ADAPTATAION_LOOPBACK_SRC_PORT;
	cvs_set_loopback_cmd.hdr.dest_port = cvs_handle;
	cvs_set_loopback_cmd.hdr.token = 0;
	cvs_set_loopback_cmd.hdr.opcode =
		VSS_ICOMMON_CMD_SET_PARAM_V2;

	cvs_set_loopback_cmd.mem_handle = 0;
	cvs_set_loopback_cmd.mem_address_lsw = 0;
	cvs_set_loopback_cmd.mem_address_msw = 0;
	cvs_set_loopback_cmd.mem_size = 0x10;

	cvs_set_loopback_cmd.vss_set_loopback.module_id =
		VOICEPROC_MODULE_VENC;
	cvs_set_loopback_cmd.vss_set_loopback.param_id =
		VOICE_PARAM_LOOPBACK_ENABLE;
	cvs_set_loopback_cmd.vss_set_loopback.param_size = 4;
	cvs_set_loopback_cmd.vss_set_loopback.reserved = 0;
	cvs_set_loopback_cmd.vss_set_loopback.loopback_enable = mode;
	cvs_set_loopback_cmd.vss_set_loopback.reserved_field = 0;

	atomic_set(&this_cvs.state, 1);
	ret = apr_send_pkt(this_cvs.apr, (uint32_t *) &cvs_set_loopback_cmd);
	if (ret < 0) {
		pr_err("%s: sending cvs set loopback enable failed\n",
			__func__);
		goto fail;
	}
	ret = wait_event_timeout(this_cvs.wait,
		(atomic_read(&this_cvs.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;
}

int voice_sec_set_loopback_cmd(u32 session_id, uint16_t mode)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, session_id);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			ret = send_packet_loopback_cmd(v, mode);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

void voice_sec_loopback_start_cmd(u32 session_id)
{
	int ret = 0;

	if (loopback_mode == LOOPBACK_ENABLE ||
	    loopback_mode == LOOPBACK_NODELAY) {
		ret = voice_sec_set_loopback_cmd(session_id, loopback_mode);
		if (ret < 0) {
			pr_err("%s: send packet loopback cmd failed(%d)\n",
				__func__, ret);
		} else {
			pr_info("%s: enable packet loopback\n",
				__func__);
		}
	}
}

void voice_sec_loopback_end_cmd(u32 session_id)
{
	int ret = 0;

	if ((loopback_mode == LOOPBACK_DISABLE) &&
	    (loopback_prev_mode == LOOPBACK_ENABLE ||
	     loopback_prev_mode == LOOPBACK_NODELAY)) {
		ret = voice_sec_set_loopback_cmd(session_id, loopback_mode);
		if (ret < 0) {
			pr_err("%s: packet loopback disable cmd failed(%d)\n",
				__func__, ret);
		} else {
			pr_info("%s: disable packet loopback\n",
				__func__);
		}
		loopback_prev_mode = 0;
	}
}

/*//////////////////////////// CVP COMMAND //////////////////////////////*/
/* This function must be sync up from voice_get_cvp_handle() of q6voice.c */
static u16 voice_get_cvp_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: cvp_handle %d\n", __func__, v->cvp_handle);

	return v->cvp_handle;
}

static int32_t q6audio_adaptation_cvp_callback(struct apr_client_data *data,
	void *priv)
{
	int i;
	uint32_t *ptr = NULL;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_cvp.apr);

		if (this_cvp.apr) {
			apr_reset(this_cvp.apr);
			atomic_set(&this_cvp.state, 0);
			this_cvp.apr = NULL;
		}

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			common->voice[i].cvp_handle = 0;

		cal_utils_clear_cal_block_q6maps(MAX_VOICE_CAL_TYPES,
				common->cal_data);

		/* Free the ION memory and clear handles for Source Tracking */
		if (is_source_tracking_shared_memomry_allocated()) {
			msm_audio_ion_free(common->source_tracking_sh_mem.sh_mem_block.dma_buf);
			common->source_tracking_sh_mem.mem_handle = 0;
			common->source_tracking_sh_mem.sh_mem_block.dma_buf = NULL;
		}

		voc_set_error_state(data->reset_proc);

		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size >= 2 * sizeof(uint32_t)) {
			ptr = data->payload;
			pr_debug("%x %x\n", ptr[0], ptr[1]);
			if (ptr[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, ptr[0], ptr[1]);
			}

			switch (ptr[0]) {
			case VSS_ICOMMON_CMD_SET_PARAM_V2:
				pr_info("%s: VSS_ICOMMON_CMD_SET_PARAM_V2\n",
					__func__);
				atomic_set(&this_cvp.state, 0);
				wake_up(&this_cvp.wait);
				break;
			case VSS_ICOMMON_CMD_SET_UI_PROPERTY:
				pr_info("%s: VSS_ICOMMON_CMD_SET_UI_PROPERTY\n",
					__func__);
				atomic_set(&this_cvp.state, 0);
				wake_up(&this_cvp.wait);
				break;
			default:
				pr_err("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	}
	return 0;
}

static int sec_voice_send_adaptation_sound_cmd(struct voice_data *v,
			uint16_t mode, uint16_t select, int16_t *parameters)
{
	struct cvp_adaptation_sound_parm_send_cmd
		cvp_adaptation_sound_param_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_adaptation_sound_param_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_adaptation_sound_param_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_adaptation_sound_param_cmd) - APR_HDR_SIZE);
	cvp_adaptation_sound_param_cmd.hdr.src_port =
		SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_adaptation_sound_param_cmd.hdr.dest_port = cvp_handle;
	cvp_adaptation_sound_param_cmd.hdr.token = 0;
	cvp_adaptation_sound_param_cmd.hdr.opcode =
		VSS_ICOMMON_CMD_SET_PARAM_V2;
	cvp_adaptation_sound_param_cmd.mem_handle = 0;
	cvp_adaptation_sound_param_cmd.mem_address_lsw = 0;
	cvp_adaptation_sound_param_cmd.mem_address_msw = 0;
	cvp_adaptation_sound_param_cmd.mem_size = 40;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.module_id =
		VOICE_VOICEMODE_MODULE;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.param_id =
		VOICE_ADAPTATION_SOUND_PARAM;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.param_size = 28;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.reserved = 0;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.eq_mode = mode;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.select = select;

	memcpy(cvp_adaptation_sound_param_cmd.adaptation_sound_data.param,
		parameters, sizeof(int16_t)*12);

	pr_info("%s: send adaptation_sound param, mode = %d, select=%d\n",
		__func__,
		cvp_adaptation_sound_param_cmd.adaptation_sound_data.eq_mode,
		cvp_adaptation_sound_param_cmd.adaptation_sound_data.select);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr,
		(uint32_t *) &cvp_adaptation_sound_param_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_adaptation_sound_param_cmd\n",
			__func__);
		return -EINVAL;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int sec_voice_set_adaptation_sound(uint16_t mode,
	uint16_t select, int16_t *parameters)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	mutex_lock(&common->common_lock);
	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_adaptation_sound_cmd(v,
					mode, select, parameters);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&common->common_lock);
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

static int sec_voice_send_nb_mode_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_nbmode_enable_cmd cvp_nbmode_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_nbmode_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_nbmode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_nbmode_cmd) - APR_HDR_SIZE);
	cvp_nbmode_cmd.hdr.src_port =
		SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_nbmode_cmd.hdr.dest_port = cvp_handle;
	cvp_nbmode_cmd.hdr.token = 0;
	cvp_nbmode_cmd.hdr.opcode =
		VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_nbmode_cmd.cvp_set_nbmode.module_id =
		VOICE_VOICEMODE_MODULE;
	cvp_nbmode_cmd.cvp_set_nbmode.param_id =
		VOICE_NBMODE_PARAM;
	cvp_nbmode_cmd.cvp_set_nbmode.param_size = 4;
	cvp_nbmode_cmd.cvp_set_nbmode.reserved = 0;
	cvp_nbmode_cmd.cvp_set_nbmode.enable = enable;
	cvp_nbmode_cmd.cvp_set_nbmode.reserved_field = 0;

	pr_info("%s: eanble = %d\n", __func__,
					cvp_nbmode_cmd.cvp_set_nbmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_nbmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_nbmode_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_nb_mode(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	mutex_lock(&common->common_lock);
	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_nb_mode_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&common->common_lock);
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

static int sec_voice_send_spk_mode_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_spkmode_enable_cmd cvp_spkmode_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_spkmode_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_spkmode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_spkmode_cmd) - APR_HDR_SIZE);
	cvp_spkmode_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_spkmode_cmd.hdr.dest_port = cvp_handle;
	cvp_spkmode_cmd.hdr.token = 0;
	cvp_spkmode_cmd.hdr.opcode = VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_spkmode_cmd.cvp_set_spkmode.module_id = VOICE_VOICEMODE_MODULE;
	cvp_spkmode_cmd.cvp_set_spkmode.param_id = VOICE_SPKMODE_PARAM;
	cvp_spkmode_cmd.cvp_set_spkmode.param_size = 4;
	cvp_spkmode_cmd.cvp_set_spkmode.reserved = 0;
	cvp_spkmode_cmd.cvp_set_spkmode.enable = enable;
	cvp_spkmode_cmd.cvp_set_spkmode.reserved_field = 0;

	pr_info("%s: Voice Module enable = %d\n", __func__,
					cvp_spkmode_cmd.cvp_set_spkmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_spkmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_spkmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_spkmode_cmd.cvp_set_spkmode.module_id = VOICE_FVSAM_MODULE;

	pr_info("%s: FVSAM Module enable = %d\n", __func__,
					cvp_spkmode_cmd.cvp_set_spkmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_spkmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_spkmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_spkmode_cmd.cvp_set_spkmode.module_id = VOICE_WISEVOICE_MODULE;

	pr_info("%s: WiseVoice Module enable = %d\n", __func__,
					cvp_spkmode_cmd.cvp_set_spkmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_spkmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_spkmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);

		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_spk_mode(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	mutex_lock(&common->common_lock);
	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_spk_mode_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&common->common_lock);
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

static int sec_voice_send_device_info_cmd(struct voice_data *v, int device)
{
	struct cvp_set_device_info_cmd cvp_device_info_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_device_info_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_device_info_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_device_info_cmd) - APR_HDR_SIZE);
	cvp_device_info_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_device_info_cmd.hdr.dest_port = cvp_handle;
	cvp_device_info_cmd.hdr.token = 0;
	cvp_device_info_cmd.hdr.opcode =
		VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_device_info_cmd.cvp_set_device_info.module_id =
		VOICE_MODULE_SET_DEVICE;
	cvp_device_info_cmd.cvp_set_device_info.param_id =
		VOICE_MODULE_SET_DEVICE_PARAM;
	cvp_device_info_cmd.cvp_set_device_info.param_size = 4;
	cvp_device_info_cmd.cvp_set_device_info.reserved = 0;
	cvp_device_info_cmd.cvp_set_device_info.enable = device;
	cvp_device_info_cmd.cvp_set_device_info.reserved_field = 0;

	pr_info("%s(): voice device info = %d\n",
		__func__, cvp_device_info_cmd.cvp_set_device_info.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_device_info_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_set_device_info_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_device_info(short device)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	mutex_lock(&common->common_lock);
	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_device_info_cmd(v, device);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&common->common_lock);
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

int sec_voice_get_loopback_enable(void)
{
	return loopback_mode;
}

void sec_voice_set_loopback_enable(int mode)
{
	loopback_prev_mode = loopback_mode;

	if (mode >= LOOPBACK_MAX ||
	    mode < LOOPBACK_DISABLE) {
		pr_err("%s : out of range, mode = %d\n",
			__func__, mode);
		loopback_mode = LOOPBACK_DISABLE;
	} else {
		loopback_mode = mode;
	}

	pr_info("%s : prev_mode = %d, mode = %d\n",
		__func__,
		loopback_prev_mode,
		loopback_mode);
}

static int sec_voice_adaptation_sound_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_adaptation_sound_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0;

	int adaptation_sound_mode = ucontrol->value.integer.value[0];
	int adaptation_sound_select = ucontrol->value.integer.value[1];
	short adaptation_sound_param[12] = {0,};

	for (i = 0; i < 12; i++) {
		adaptation_sound_param[i] =
			(short)ucontrol->value.integer.value[2+i];
		pr_debug("sec_voice_adaptation_sound_put : param - %d\n",
				adaptation_sound_param[i]);
	}

	return sec_voice_set_adaptation_sound(adaptation_sound_mode,
				adaptation_sound_select,
				adaptation_sound_param);
}

static int sec_voice_nb_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_nb_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_debug("%s: enable=%d\n", __func__, enable);

	return sec_voice_set_nb_mode(enable);
}

static int sec_voice_spk_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_spk_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	pr_debug("%s: enable=%d\n", __func__, enable);

	return sec_voice_set_spk_mode(enable);
}

static int sec_voice_loopback_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int loopback_enable = ucontrol->value.integer.value[0];

	pr_debug("%s: loopback enable=%d\n", __func__, loopback_enable);

	sec_voice_set_loopback_enable(loopback_enable);
	return 0;
}

static int sec_voice_loopback_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = sec_voice_get_loopback_enable();
	return 0;
}

static const char * const voice_device[] = {
	"ETC", "SPK", "EAR", "BT", "RCV"
};

static const struct soc_enum sec_voice_device_enum[] = {
	SOC_ENUM_SINGLE_EXT(5, voice_device),
};

static int sec_voice_dev_info_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_voice_dev_info_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int voc_dev = ucontrol->value.integer.value[0];

	pr_debug("%s: voice device=%d\n", __func__, voc_dev);

	return sec_voice_set_device_info(voc_dev);
}
/*******************************************************/
/*/////////////////////// COMMON //////////////////////*/
/*******************************************************/
static const struct snd_kcontrol_new samsung_solution_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("SA data", SND_SOC_NOPM, 0, 65535, 0, 27,
				sec_audio_sound_alive_get,
				sec_audio_sound_alive_put),
	SOC_SINGLE_MULTI_EXT("VSP data", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_play_speed_get,
				sec_audio_play_speed_put),
	SOC_SINGLE_MULTI_EXT("Audio DHA data", SND_SOC_NOPM, 0, 65535, 0, 14,
				sec_audio_adaptation_sound_get,
				sec_audio_adaptation_sound_put),
	SOC_SINGLE_MULTI_EXT("LRSM data", SND_SOC_NOPM, 0, 65535, 0, 2,
				sec_audio_sound_balance_get,
				sec_audio_sound_balance_put),
	SOC_SINGLE_EXT("VoiceTrackInfo", SND_SOC_NOPM, 0, 2147483647, 0,
				sec_audio_voice_tracking_info_get,
				sec_audio_voice_tracking_info_put),
	SOC_SINGLE_MULTI_EXT("MSP data", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_myspace_get, sec_audio_myspace_put),
	SOC_SINGLE_MULTI_EXT("SB enable", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_sound_boost_get,
				sec_audio_sound_boost_put),
	SOC_SINGLE_MULTI_EXT("UPSCALER", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_upscaler_get, sec_audio_upscaler_put),
	SOC_SINGLE_MULTI_EXT("SB rotation", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_sb_rotation_get, sec_audio_sb_rotation_put),
	SOC_SINGLE_MULTI_EXT("Sec Set DHA data", SND_SOC_NOPM, 0, 65535, 0, 14,
				sec_voice_adaptation_sound_get,
				sec_voice_adaptation_sound_put),
	SOC_SINGLE_EXT("NB Mode", SND_SOC_NOPM, 0, 1, 0,
				sec_voice_nb_mode_get, sec_voice_nb_mode_put),
	SOC_SINGLE_EXT("Speaker Sensor Mode", SND_SOC_NOPM, 0, 1, 0,
				sec_voice_spk_mode_get, sec_voice_spk_mode_put),
	SOC_SINGLE_EXT("Loopback Enable", SND_SOC_NOPM, 0, LOOPBACK_MAX, 0,
				sec_voice_loopback_get, sec_voice_loopback_put),
	SOC_ENUM_EXT("Voice Device Info", sec_voice_device_enum[0],
				sec_voice_dev_info_get, sec_voice_dev_info_put),
};

static int q6audio_adaptation_component_probe(struct snd_soc_component *component)
{
	return snd_soc_add_component_controls(component,
			samsung_solution_mixer_controls,
			ARRAY_SIZE(samsung_solution_mixer_controls));
}

static struct snd_soc_component_driver q6audio_adaptation_component = {
	.name	= "q6audio-adaptation",
	.probe	= q6audio_adaptation_component_probe,
};

static int samsung_q6audio_adaptation_probe(struct platform_device *pdev)
{
	int ret;

	mutex_init(&asm_lock);
	atomic_set(&this_cvs.state, 0);
	atomic_set(&this_cvp.state, 0);
	init_waitqueue_head(&this_cvs.wait);
	init_waitqueue_head(&this_cvp.wait);
	common = voice_get_common_data();

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,voice-tracking-tx-port-id",
			&afe_port.voice_tracking_id);
	if (ret)
		dev_err(&pdev->dev, "Unable to read voice-tracking tx port\n");

	return snd_soc_register_component(&pdev->dev,
			&q6audio_adaptation_component, NULL, 0);
}

static int samsung_q6audio_adaptation_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id samsung_q6audio_adaptation_dt_match[] = {
	{ .compatible = "samsung,q6audio-adaptation" },
	{}
};
MODULE_DEVICE_TABLE(of, samsung_q6audio_adaptation_dt_match);

static struct platform_driver samsung_q6audio_adaptation_driver = {
	.driver = {
		.name = "samsung-q6audio-adaptation",
		.of_match_table = samsung_q6audio_adaptation_dt_match,
	},
	.probe = samsung_q6audio_adaptation_probe,
	.remove = samsung_q6audio_adaptation_remove,
};
module_platform_driver(samsung_q6audio_adaptation_driver);

MODULE_DESCRIPTION("Samsung ASoC ADSP sound adaptation (ASM controls)");
MODULE_LICENSE("GPL v2");
