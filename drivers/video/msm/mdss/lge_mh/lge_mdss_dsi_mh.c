#include <linux/delay.h>
#include "mdss_dsi.h"

#include "lge/mfts_mode.h"

#include <soc/qcom/lge/board_lge.h>

#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
#include "mdss_mdp.h"
#include "lge/reader_mode.h"
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
#include <linux/mfd/sm5107.h>
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
extern int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					bool active);
#endif

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *pcmds, u32 flags);
extern int mdss_dsi_parse_dcs_cmds(struct device_node *np, struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);


#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
int lge_mdss_dsi_panel_power_dsv_ctrl(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable)
{
	int rc = 0;

	if (!ctrl_pdata) {
		pr_err("%s: invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	if(enable) {
		if (lge_get_display_power_ctrl()) {
			lge_extra_gpio_set_value(ctrl_pdata, "dsv_ena", enable);
		}

		rc += ext_dsv_register_set(SM5107_CONTROL, 0x03);
		rc += ext_dsv_register_set(SM5107_CTRL_SET, 0x40);

		usleep_range(15000, 15000);
	} else {
		//rc += ext_dsv_register_set(SM5107_CONTROL, 0x00); //will be decided later
		//rc += ext_dsv_register_set(SM5107_CTRL_SET, 0x00);

		if (lge_get_display_power_ctrl()) {
			lge_extra_gpio_set_value(ctrl_pdata, "dsv_ena", enable);
		}

		usleep_range(5000, 5000);
	}
	pr_err("%s: enabled[%d], result[%d] \n", __func__, enable, rc);

	return rc;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_RESET)
static int mdss_dsi_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		goto rst_gpio_err;
	}

	rc = lge_extra_gpio_request(ctrl_pdata, "vddio");
	if (rc) {
		pr_err("request vddio gpio failed, rc=%d\n",
			rc);
		goto vddio_gpio_err;
	}

	rc = lge_extra_gpio_request(ctrl_pdata, "avdd");
	if (rc) {
		pr_err("request avdd gpio failed, rc=%d\n",
			rc);
		goto avdd_gpio_err;
	}

	rc = lge_extra_gpio_request(ctrl_pdata, "dsv_ena");
	if (rc) {
		pr_err("request dsv_ena gpio failed, rc=%d\n",
			rc);
		goto dsv_ena_gpio_err;
	}

	return rc;

dsv_ena_gpio_err:
	lge_extra_gpio_free(ctrl_pdata, "avdd");
avdd_gpio_err:
	lge_extra_gpio_free(ctrl_pdata, "vddio");
vddio_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
rst_gpio_err:

	return rc;
}

int mdss_dsi_panel_reset_lgd_incell_sw49106(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("%s: Invalid pinfo data\n", __func__);
		return -EINVAL;
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return rc;
	}

	pr_info("%s: + enable = %d (override: mh)\n", __func__, enable);

	if (enable) {
		if (!pinfo->cont_splash_enabled) {
			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);

				usleep_range(1000, 1000);
				lge_extra_gpio_set_value(ctrl_pdata, "touch_reset", pdata->panel_info.rst_seq[i]);

				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
			}
			pr_info("%s: LCD/Touch reset sequence done \n", __func__);
		}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
		lge_mdss_dsi_panel_power_dsv_ctrl(ctrl_pdata, enable);
#endif

		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
		lge_mdss_dsi_panel_power_dsv_ctrl(ctrl_pdata, enable);
#endif

		if (lge_get_display_power_ctrl()) {
			lge_extra_gpio_set_value(ctrl_pdata, "touch_reset", enable);
			usleep_range(1000, 1000);

			gpio_set_value((ctrl_pdata->rst_gpio), enable);
			usleep_range(5000, 5000);
		}
	}

	pr_info("%s: -\n", __func__);

	return rc;
}

int mdss_dsi_panel_reset_jdi_nt35596(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if ((mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) ||
			pinfo->is_dba_panel) {
		pr_debug("%s:%d, right ctrl gpio configuration not needed\n",
			__func__, __LINE__);
		return rc;
	}
	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n", __func__, __LINE__);
		return rc;
	}

	pr_info("%s: + enable = %d (override: mh)\n", __func__, enable);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("request gpio failed, rc=%d\n", rc);
			return rc;
		}
		if (!pinfo->cont_splash_enabled) {
			if (pdata->panel_info.rst_seq_len) {
				rc = gpio_direction_output(ctrl_pdata->rst_gpio,
					pdata->panel_info.rst_seq[0]);
				if (rc) {
					pr_err("%s: unable to set dir for rst gpio\n",
						__func__);
					goto exit;
				}
			}

			if(lge_get_display_power_ctrl()) {
				pr_info("%s: turn panel power on\n", __func__);
				lge_extra_gpio_set_value(ctrl_pdata, "avdd", 1);
				usleep_range(1000, 1000);
				lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
				usleep_range(1000, 1000);
				lge_extra_gpio_set_value(ctrl_pdata, "dsv_ena", 1);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
				ext_dsv_mode_change(MODE_NORMAL); // DSV_ENREG enabled
				usleep_range(15000, 15000);
#endif
			} else {
#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
				ext_dsv_mode_change(MODE_NORMAL); // DSV_ENREG enabled
#endif
				pr_info("%s: skip panel power control\n", __func__);
			}

			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
			}
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
		ext_dsv_mode_change(POWER_OFF);	//DSV external pin control, active-discharge enable
#endif

		if(lge_get_display_power_ctrl()) {
			pr_info("%s: turn panel power off\n", __func__);

			gpio_set_value(ctrl_pdata->rst_gpio, 0);
			usleep_range(5000, 5000);

			lge_extra_gpio_set_value(ctrl_pdata, "dsv_ena", 0); // DSV low
			usleep_range(10000, 10000);

			lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0); // vddio low
			usleep_range(1000, 1000);
			lge_extra_gpio_set_value(ctrl_pdata, "avdd", 0); // avdd low
		}

		lge_extra_gpio_free(ctrl_pdata, "dsv_ena");
		lge_extra_gpio_free(ctrl_pdata, "avdd");
		lge_extra_gpio_free(ctrl_pdata, "vddio");
		gpio_free(ctrl_pdata->rst_gpio);
	}

	pr_info("%s: -\n", __func__);
exit:
	return rc;
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;

	switch(lge_get_panel_type()) {
	case LGD_INCELL_SW49106:
		rc = mdss_dsi_panel_reset_lgd_incell_sw49106(pdata, enable);
		break;
	case LV9_JDI_NT35596:
		rc = mdss_dsi_panel_reset_jdi_nt35596(pdata, enable);
		break;
	default:
		break;
	}

	return rc;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on_lgd_incell_sw49106(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid pdata\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: + (override: mh)\n", __func__);

	if (lge_get_display_power_ctrl()) {
		lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
	}

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		return ret;
	}

	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	pr_info("%s: -\n", __func__);

	return ret;
}

int mdss_dsi_panel_power_on_jdi_nt35596(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: (override: mh)\n", __func__);

	if(lge_get_display_power_ctrl()) {
		ret = msm_dss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 1);

		if (ret) {
			pr_err("%s: failed to enable vregs for %s\n",
					__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
			return ret;
		}
	}

	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	return ret;
}

int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;

	switch(lge_get_panel_type()) {
	case LGD_INCELL_SW49106:
		ret = mdss_dsi_panel_power_on_lgd_incell_sw49106(pdata);
		break;
	case LV9_JDI_NT35596:
		ret = mdss_dsi_panel_power_on_jdi_nt35596(pdata);
		break;
	default:
		break;
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off_lgd_incell_sw49106(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid pdata\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: + (override: mh)\n", __func__);

	ret = mdss_dsi_panel_reset(pdata, 0);
	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

	if (lge_get_display_power_ctrl()) {
		lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	}

	pr_info("%s: -\n", __func__);
end:
	return ret;
}

int mdss_dsi_panel_power_off_jdi_nt35596(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: (override: mh)\n", __func__);

	ret = mdss_dsi_panel_reset(pdata, 0);
	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	if(lge_get_display_power_ctrl()) {
		ret = msm_dss_enable_vreg(
				ctrl_pdata->panel_power_data.vreg_config,
				ctrl_pdata->panel_power_data.num_vreg, 0);

		if (ret)
			pr_err("%s: failed to disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
	} else {
		pr_info("%s: keep panel power for lpwg mode\n", __func__);
	}

end:
	return ret;
}

int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;

	switch(lge_get_panel_type()) {
	case LGD_INCELL_SW49106:
		ret = mdss_dsi_panel_power_off_lgd_incell_sw49106(pdata);
		break;
	case LV9_JDI_NT35596:
		ret = mdss_dsi_panel_power_off_jdi_nt35596(pdata);
		break;
	default:
		break;
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
void mdss_dsi_ctrl_shutdown_lgd_incell_sw49106(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return;
	}

	lge_extra_gpio_set_value(ctrl_pdata, "dsv_ena", 0);
	usleep_range(5000, 5000);

	lge_extra_gpio_set_value(ctrl_pdata, "touch_reset", 0);
	usleep_range(1000, 1000);

	gpio_set_value((ctrl_pdata->rst_gpio), 0);
	usleep_range(5000, 5000);

	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	usleep_range(1000, 1000);

	pr_info("%s: panel shutdown done \n", __func__);

	return;
}

void mdss_dsi_ctrl_shutdown_jdi_nt35596(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return;
	}

	if(gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_info("%s: reset to low\n", __func__);
		gpio_set_value((ctrl_pdata->rst_gpio), 0); //Reset low
	}
	usleep_range(10000, 10000);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_SM5107_DSV)
	ext_dsv_mode_change(POWER_OFF); 	 //Disable DSV_EN
	usleep_range(2000, 2000);
#endif
	lge_extra_gpio_set_value(ctrl_pdata, "dsv_ena", 0);  //DSV low
	usleep_range(10000, 10000);
	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);	 //vddio low
	usleep_range(5000, 5000);
	lge_extra_gpio_set_value(ctrl_pdata, "avdd", 0);	//avdd low
	pr_info("%s: turn panel shutdown\n", __func__);

	return;
}

void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	switch(lge_get_panel_type()) {
	case LGD_INCELL_SW49106:
		mdss_dsi_ctrl_shutdown_lgd_incell_sw49106(pdev);
		break;
	case LV9_JDI_NT35596:
		mdss_dsi_ctrl_shutdown_jdi_nt35596(pdev);
		break;
	default:
		break;
	}
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
extern int mdss_dsi_parse_dcs_cmds(struct device_node *np, struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);
extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *pcmds, u32 flags);

static struct dsi_panel_cmds reader_mode_cmds[4];

int lge_mdss_dsi_parse_reader_mode_cmds(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_OFF], "qcom,panel-reader-mode-off-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_STEP_1], "qcom,panel-reader-mode-step1-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_STEP_2], "qcom,panel-reader-mode-step2-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_STEP_3], "qcom,panel-reader-mode-step3-command", "qcom,mdss-dsi-reader-mode-command-state");

    {
        int i;
        for (i=0; i<4; ++i) {
            pr_info("%s: cmd size[%d] = %d\n", __func__, i, reader_mode_cmds[i].cmd_cnt);
        }
    }

	return 0;
}

static bool change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int new_mode)
{
	if (new_mode == READER_MODE_MONO) {
		pr_info("%s: READER_MODE_MONO is not supported. reader mode is going off.\n", __func__);
		new_mode = READER_MODE_STEP_2;
	}

    pr_info("%s ++\n", __func__);
	if(reader_mode_cmds[new_mode].cmd_cnt) {
		pr_info("%s: sending reader mode commands [%d]\n", __func__, new_mode);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_cmds[new_mode], CMD_REQ_COMMIT);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	}
    pr_info("%s --\n", __func__);
	return true;
}

bool lge_change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int old_mode, int new_mode)
{
	if (old_mode == new_mode) {
		pr_info("%s: same mode [%d]\n", __func__, new_mode);
		return true;
	}

	return change_reader_mode(ctrl, new_mode);
}

int lge_mdss_dsi_panel_send_post_on_cmds(struct mdss_dsi_ctrl_pdata *ctrl, int cur_mode)
{
	if (cur_mode != READER_MODE_OFF)
		change_reader_mode(ctrl, cur_mode);
	return 0;
}
#endif
