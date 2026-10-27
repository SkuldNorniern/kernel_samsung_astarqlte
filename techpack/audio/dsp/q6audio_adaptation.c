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
#include "../asoc/msm-pcm-routing-v2.h"

#define SEC_ADAPTATAION_AUDIO_PORT	3	/* ASM */

static struct mutex asm_lock;
static u32 voice_tracking_id;

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
static struct audio_client *sec_audio_get_asm_client(void)
{
	struct msm_pcm_routing_fdai_data fe_dai_map;

	msm_pcm_routing_get_fedai_info(SEC_ADAPTATAION_AUDIO_PORT,
			SESSION_TYPE_RX, &fe_dai_map);
	return q6asm_get_audio_client(fe_dai_map.strm_id);
}

static int sec_audio_sound_alive_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_sound_alive(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_play_speed_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_play_speed(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_adaptation_sound_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_adaptation_sound(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sound_balance_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_sound_balance(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_myspace_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_myspace(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sound_boost_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_sound_boost(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_upscaler_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_upscaler(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_sb_rotation_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;

	mutex_lock(&asm_lock);
	ret = q6asm_set_sb_rotation(sec_audio_get_asm_client(),
			(long *)ucontrol->value.integer.value);
	mutex_unlock(&asm_lock);
	return ret;
}

static int sec_audio_voice_tracking_info_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int be_idx = 0;
	char *param_value;
	int *update_param_value;
	uint32_t param_size = (RMS_PAYLOAD_LEN + 1) * sizeof(uint32_t);
	struct msm_pcm_routing_bdai_data msm_bedai = {0};
	struct param_hdr_v3 param_hdr;

	param_value = kzalloc(param_size, GFP_KERNEL);
	if (!param_value)
		return -ENOMEM;

	for (be_idx = 0; be_idx < MSM_BACKEND_DAI_MAX; be_idx++) {
		msm_pcm_routing_get_bedai_info(be_idx, &msm_bedai);
		if (msm_bedai.port_id == voice_tracking_id)
			break;
	}
	if ((be_idx < MSM_BACKEND_DAI_MAX) && msm_bedai.active) {
		memset(&param_hdr, 0, sizeof(param_hdr));
		param_hdr.module_id = ADM_MODULE_ID_PP_SS_REC;
		param_hdr.instance_id = 0x8000;
		param_hdr.param_id = ADM_PARAM_ID_PP_SS_REC_GETPARAMS;
		param_hdr.param_size = param_size;
		rc = adm_get_pp_params((u16)voice_tracking_id, 0,
				ADM_CLIENT_ID_DEFAULT, NULL, &param_hdr,
				param_value);
		if (rc) {
			pr_err("%s: get parameters failed:%d\n", __func__, rc);
			kfree(param_value);
			return -EINVAL;
		}
		update_param_value = (int *)param_value;
		ucontrol->value.integer.value[0] = update_param_value[0];
	}
	kfree(param_value);

	return 0;
}

static int sec_audio_voice_tracking_info_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int sec_audio_asm_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static const struct snd_kcontrol_new samsung_solution_mixer_controls[] = {
	SOC_SINGLE_MULTI_EXT("SA data", SND_SOC_NOPM, 0, 65535, 0, 27,
				sec_audio_asm_get, sec_audio_sound_alive_put),
	SOC_SINGLE_MULTI_EXT("VSP data", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_asm_get, sec_audio_play_speed_put),
	SOC_SINGLE_MULTI_EXT("Audio DHA data", SND_SOC_NOPM, 0, 65535, 0, 14,
				sec_audio_asm_get, sec_audio_adaptation_sound_put),
	SOC_SINGLE_MULTI_EXT("LRSM data", SND_SOC_NOPM, 0, 65535, 0, 2,
				sec_audio_asm_get, sec_audio_sound_balance_put),
	SOC_SINGLE_EXT("VoiceTrackInfo", SND_SOC_NOPM, 0, 2147483647, 0,
				sec_audio_voice_tracking_info_get,
				sec_audio_voice_tracking_info_put),
	SOC_SINGLE_MULTI_EXT("MSP data", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_asm_get, sec_audio_myspace_put),
	SOC_SINGLE_MULTI_EXT("SB enable", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_asm_get, sec_audio_sound_boost_put),
	SOC_SINGLE_MULTI_EXT("UPSCALER", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_asm_get, sec_audio_upscaler_put),
	SOC_SINGLE_MULTI_EXT("SB rotation", SND_SOC_NOPM, 0, 65535, 0, 1,
				sec_audio_asm_get, sec_audio_sb_rotation_put),
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

	ret = of_property_read_u32(pdev->dev.of_node,
			"adaptation,voice-tracking-tx-port-id",
			&voice_tracking_id);
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
