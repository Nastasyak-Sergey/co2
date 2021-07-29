#ifndef ERRORS_H
#define ERRORS_H
/**
 * errors - provides definitions for all error codes
 *
 * User may check corresponding error codes against this definitions.
 */

enum err_t {
	/** No error, everything fine */
	EOK = 0,
	/** Unknown error. Something went wrong but don't boter detecting what */
	EUNKNOWN = -1,
	/** Some feature is not implemented yet */
	ENOTIMPLEMENTED = -2,
	/** Something simply could not be done */
	EUNAVAILABLE = -3,
	/** Wrong argument passed */
	EWRONGARG = -4,
	/** Not enough arguments */
	ENENARG = -5,
	/** Range exceeds available limits */
	ERANGE = -6,
	/** Data storage is full */
	EFULL = -7,
	/** Data storage is empty */
	EEMPTY = -8
};


// alias type for more concise definitions
typedef enum err_t err_t;
#endif
