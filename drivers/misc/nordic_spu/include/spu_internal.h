/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_INCLUDE_DRIVERS_SPU_INTERNAL_H__
#define NRF_INCLUDE_DRIVERS_SPU_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Call macro @p F with the elements at indices @p idx_0 and @p idx_1 in array property
 *          @p prop, but only if the property exists in node @p inst and has the given indices.
 *
 *          F should have the signature F(val_0, val_1, ...).
 *          Additional arguments given to this macro are passed on to F.
 *
 * @param   F        Macro to apply.
 * @param   sep      Separator placed before the expansion of F(...) (e.g. comma or semicolon).
 *                   Must be in parentheses to enable providing a comma as separator.
 * @param   inst     Node instance containing the array property.
 * @param   prop     Name of the array property.
 * @param   idx_0    The index of @p prop where the first element is located.
 * @param   idx_1    The index of @p prop where the second element is located.
 */
#define SPU_ARRAY_PROP_PAIR_APPLY(F, sep, inst, prop, idx_0, idx_1, ...)                           \
	IF_ENABLED(UTIL_AND(DT_INST_PROP_HAS_IDX(inst, prop, idx_0),                               \
			    DT_INST_PROP_HAS_IDX(inst, prop, idx_1)),                              \
		   (__DEBRACKET sep F(DT_INST_PROP_BY_IDX(inst, prop, idx_0),                      \
				      DT_INST_PROP_BY_IDX(inst, prop, idx_1), __VA_ARGS__)))

/**
 * @brief   Apply macro @p F to up to 5 pairs of consecutive pairs in an array property @p prop.
 *
 *          F should have the signature F(val_0, val_1, ...).
 *          Additional arguments given to this macro are passed on to F.
 *
 * @param   F        Macro to apply.
 * @param   sep      Separator between pairs (e.g. comma or semicolon). Must be in parentheses
 *                   to enable providing a comma as separator.
 * @param   inst     Node instance containing the array property.
 * @param   prop     Name of the array property.
 */
#define SPU_ARRAY_PROP_PAIRWISE_MAP(F, sep, inst, prop, ...)                                       \
	SPU_ARRAY_PROP_PAIR_APPLY(F, (), inst, prop, 0, 1, __VA_ARGS__)                            \
	SPU_ARRAY_PROP_PAIR_APPLY(F, sep, inst, prop, 2, 3, __VA_ARGS__)                           \
	SPU_ARRAY_PROP_PAIR_APPLY(F, sep, inst, prop, 4, 5, __VA_ARGS__)                           \
	SPU_ARRAY_PROP_PAIR_APPLY(F, sep, inst, prop, 6, 7, __VA_ARGS__)                           \
	SPU_ARRAY_PROP_PAIR_APPLY(F, sep, inst, prop, 8, 9, __VA_ARGS__)

/* clang-format off */

/**
 * @brief   Generate an array of remapped ID structs based on a pairwise-mapping of integers in its
 *          devicetree property.
 *
 * @param   inst    Node instance containing the property to parse.
 */
#define SPU_GENERATE_REMAPPED_ID_ARRAY(inst, ...)                                                       \
        SPU_ARRAY_PROP_PAIRWISE_MAP(SPU_REMAPPED_PERIPH_ID_INIT, (,), inst, remapped_periph_ids, __VA_ARGS__)

/* clang-format on */

#ifdef __cplusplus
}
#endif

#endif /* NRF_INCLUDE_DRIVERS_SPU_INTERNAL_H__ */
